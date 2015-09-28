#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "flags.h"
#include "wrr.h"

#define max(arg1,arg2) (arg1>arg2 ? arg1 : arg2)
#define min(arg1,arg2) (arg1<arg2 ? arg1 : arg2)


static class WRRClass : public TclClass
{
	public:
		WRRClass() : TclClass("Queue/WRR") {}
		TclObject* create(int argc, const char*const* argv)
		{
			return (new WRR);
		}
} class_dwrr;

WRR::WRR()
{
	queues=new PacketWRR[MAX_QUEUE_NUM];
	round_time=0;
	last_idle_time=0;
	init=false;
	current=0;

	total_qlen_tchan_=NULL;
	qlen_tchan_=NULL;

	queue_num_=8;
	mean_pktsize_=1500;
	port_thresh_=65;
	marking_scheme_=0;
	estimate_round_alpha_=0.75;
	estimate_round_idle_interval_bytes_=1500;
	link_capacity_=10000000000;
	debug_=0;

	/* bind variables */
	bind("queue_num_", &queue_num_);
	bind("mean_pktsize_", &mean_pktsize_);
	bind("port_thresh_",&port_thresh_);
	bind("marking_scheme_",&marking_scheme_);
	bind("estimate_round_alpha_",&estimate_round_alpha_);
	bind("estimate_round_idle_interval_bytes_",&estimate_round_idle_interval_bytes_);
	bind_bw("link_capacity_",&link_capacity_);
	bind_bool("debug_",&debug_);
}

WRR::~WRR()
{
	delete [] queues;
}

/* Get total length of all queues in bytes */
int WRR::TotalByteLength()
{
	int result=0;
	for(int i=0;i<queue_num_;i++)
	{
		result+=queues[i].byteLength();
	}
	return result;
}

/* Determine whether we need to mark ECN where q is current queue number. Return 1 if it requires marking */
int WRR::MarkingECN(int q)
{
	if(q<0||q>=queue_num_)
	{
		fprintf (stderr,"illegal queue number\n");
		exit (1);
	}

	/* Per-queue ECN marking */
	if(marking_scheme_==PER_QUEUE_MARKING)
	{
		if(queues[q].byteLength()>queues[q].thresh*mean_pktsize_)
			return 1;
		else
			return 0;
	}
	/* Per-port ECN marking */
	else if(marking_scheme_==PER_PORT_MARKING)
	{
		if(TotalByteLength()>port_thresh_*mean_pktsize_)
			return 1;
		else
			return 0;
	}
	/* MQ-ECN for round robin packet scheduling algorithms */
	else if(marking_scheme_==MQ_MARKING_RR)
	{
		double thresh=0;
		if(round_time>=0.000000001&&link_capacity_>0)
			thresh=min(queues[q].quantum*8/round_time/link_capacity_,1)*port_thresh_;
		else
			thresh=port_thresh_;

		//For debug
		if(debug_)
			printf("round time: %.9f threshold of queue %d: %f\n",round_time, q, thresh);

		if(queues[q].byteLength()>thresh*mean_pktsize_)
			return 1;
		else
			return 0;
	}
	/* Unknown ECN marking scheme */
	else
	{
		fprintf (stderr,"Unknown ECN marking scheme\n");
		return 0;
	}
}

/*
 *  entry points from OTcL to set per queue state variables
 *   - $q set-quantum queue_id queue_quantum (quantum is actually weight)
 *   - $q set-thresh queue_id queue_thresh
 *   - $q attach-total file
 *	  - $q attach-queue file
 *
 *  NOTE: $q represents the discipline queue variable in OTcl.
 */
int WRR::command(int argc, const char*const* argv)
{
	if(argc==3)
	{
		// attach a file to trace total queue length
		if (strcmp(argv[1], "attach-total") == 0)
		{
			int mode;
			const char* id = argv[2];
			Tcl& tcl = Tcl::instance();
			total_qlen_tchan_=Tcl_GetChannel(tcl.interp(), (char*)id, &mode);
			if (total_qlen_tchan_==0)
			{
				tcl.resultf("WRR: trace: can't attach %s for writing", id);
				return (TCL_ERROR);
			}
			return (TCL_OK);
		}
		else if (strcmp(argv[1], "attach-queue") == 0)
		{
			int mode;
			const char* id = argv[2];
			Tcl& tcl = Tcl::instance();
			qlen_tchan_=Tcl_GetChannel(tcl.interp(), (char*)id, &mode);
			if (qlen_tchan_==0)
			{
				tcl.resultf("WRR: trace: can't attach %s for writing", id);
				return (TCL_ERROR);
			}
			return (TCL_OK);
		}
	}
	else if (argc == 4)
	{
		if (strcmp(argv[1], "set-quantum")==0)
		{
			int queue_id=atoi(argv[2]);
			if(queue_id<queue_num_&&queue_id>=0)
			{
				int quantum=atoi(argv[3]);
				if(quantum>0)
				{
					queues[queue_id].quantum=quantum;
					return (TCL_OK);
				}
				else
				{
					fprintf (stderr,"illegal quantum value %s for queue %s\n", argv[3], argv[2]);
					exit (1);
				}
			}
			/* Exceed the maximum queue number or smaller than 0*/
			else
			{
				fprintf (stderr,"no such queue %s\n",argv[2]);
				exit (1);
			}
		}
		else if(strcmp(argv[1], "set-thresh")==0)
		{
			int queue_id=atoi(argv[2]);
			if(queue_id<queue_num_&&queue_id>=0)
			{
				double thresh=atof(argv[3]);
				if(thresh>=0)
				{
					queues[queue_id].thresh=thresh;
					return (TCL_OK);
				}
				else
				{
					fprintf (stderr,"illegal thresh value %s for queue %s\n", argv[3],argv[2]);
					exit (1);
				}
			}
			/* Exceed the maximum queue number or smaller than 0*/
			else
			{
				fprintf (stderr,"no such queue %s\n",argv[2]);
				exit (1);
			}
		}
	}
	return (Queue::command(argc, argv));
}

/* Receive a new packet */
void WRR::enque(Packet *p)
{
	int i=0;
	hdr_ip *iph=hdr_ip::access(p);
	int prio=iph->prio();
	hdr_flags* hf=hdr_flags::access(p);
	int pktSize=hdr_cmn::access(p)->size();
	int qlimBytes=qlim_*mean_pktsize_;
	/* 1<=queue_num_<=MAX_QUEUE_NUM */
	queue_num_=max(min(queue_num_,MAX_QUEUE_NUM),1);

	if(TotalByteLength()==0)
	{
		double now=Scheduler::instance().clock();
		double idleTime=now-last_idle_time;
		int intervalNum=0;
		if(estimate_round_idle_interval_bytes_>0 && link_capacity_>0)
		{
			intervalNum=int(idleTime/(estimate_round_idle_interval_bytes_*8/link_capacity_));
			round_time=round_time*pow(estimate_round_alpha_,intervalNum);
		}
		else
		{
			round_time=0;
		}

		/* Reset start time for all queues */
		for(i=0;i<queue_num_;i++)
			queues[i].start_time=now;

		if(debug_)
			printf("%.9f smooth round time is reset to %f after %d idle time slots\n",now, round_time, intervalNum);

	}

	/* The shared buffer is overfilld */
	if(TotalByteLength()+pktSize>qlimBytes)
	{
		drop(p);
		//printf("Packet drop\n");
		return;
	}

	if(prio>=queue_num_||prio<0)
		prio=queue_num_-1;

	queues[prio].enque(p);
	/* Enqueue ECN marking */
	if(MarkingECN(prio)>0&&hf->ect())
		hf->ce() = 1;

	trace_qlen();
	trace_total_qlen();
}

Packet *WRR::deque(void)
{
	Packet *pkt=NULL;
	int pktSize=0;
	double now=Scheduler::instance().clock();

	/*At least one queue is active*/
	if(TotalByteLength()>0)
	{
		while(1)
		{
			/* If current queue is active */
			if(queues[current].length()>0)
			{
				if(queues[current].counter_updated==false)
				{
					queues[current].counter_updated=true;
					queues[current].counter=queues[current].quantum;
				}

				pkt=queues[current].head();
				pktSize=hdr_cmn::access(pkt)->size();

				/* We have enough quantum to dequeue this packet */
				if(pktSize<=queues[current].counter)
				{
					pkt=queues[current].deque();
					queues[current].counter-=pktSize;

					/* After dequeue, current queue becomes empty */
					if(queues[current].length()==0)
					{
						double round_time_sample=now-queues[current].start_time+pktSize*8/link_capacity_;
						round_time=round_time*estimate_round_alpha_+round_time_sample*(1-estimate_round_alpha_);
						if(debug_&&marking_scheme_==MQ_MARKING_RR)
							printf("sample round time: %.9f round time: %.9f\n",round_time_sample,round_time);

						/* Move to next queue */
						queues[current].start_time=now+pktSize*8/link_capacity_;
						queues[current].counter_updated=false;
						current=(current+1)%queue_num_;
					}

					break;
				}
				/* No enough quantum to dequeue this packet */
				else
				{
					double round_time_sample=now-queues[current].start_time;
					round_time=round_time*estimate_round_alpha_+round_time_sample*(1-estimate_round_alpha_);
					if(debug_&&marking_scheme_==MQ_MARKING_RR)
						printf("sample round time: %.9f round time: %.9f\n",round_time_sample,round_time);

					queues[current].start_time=now;
					queues[current].counter_updated=false;
					current=(current+1)%queue_num_;
				}
			}
			/* Empty queue! No need to do any sample. Just move to next queue */
			else
			{
				queues[current].start_time=now;
				queues[current].counter_updated=false;
				current=(current+1)%queue_num_;
			}
		}
	}

	if(TotalByteLength()==0)
		last_idle_time=now;

	return pkt;
}

/* routine to write total qlen records */
void WRR::trace_total_qlen()
{
	if (total_qlen_tchan_)
	{
		char wrk[500]={0};
		int n;
		double t = Scheduler::instance().clock();
		sprintf(wrk, "%g, %d", t,TotalByteLength());
		n=strlen(wrk);
		wrk[n] = '\n';
		wrk[n+1] = 0;
		(void)Tcl_Write(total_qlen_tchan_, wrk, n+1);
	}
}

/* routine to write per-queue qlen records */
void WRR::trace_qlen()
{
	if (qlen_tchan_)
	{
		char wrk[500]={0};
		int n;
		double t = Scheduler::instance().clock();
		sprintf(wrk, "%g", t);
		n=strlen(wrk);
		wrk[n]=0;
		(void)Tcl_Write(qlen_tchan_, wrk, n);

		for(int i=0;i<queue_num_; i++)
		{
			sprintf(wrk, ", %d",queues[i].byteLength());
			n=strlen(wrk);
			wrk[n]=0;
			(void)Tcl_Write(qlen_tchan_, wrk, n);
		}
		(void)Tcl_Write(qlen_tchan_, "\n", 1);
	}
}
