#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "flags.h"
#include "wfq.h"

#define max(arg1,arg2) (arg1>arg2 ? arg1 : arg2)
#define min(arg1,arg2) (arg1<arg2 ? arg1 : arg2)

static class WFQClass : public TclClass
{
	public:
		WFQClass() : TclClass("Queue/WFQ") {}
		TclObject* create(int argc, const char*const* argv)
		{
			return (new WFQ);
		}
} class_wfq;

WFQ::WFQ(): timer_(this)
{
	queue_num_=32;
	mean_pktsize_=1500;
	port_thresh_=65;
	marking_scheme_=0;
	weight_sum=0;
	weight_sum_estimate=0;
	init=0;
	last_update_time=0;
	last_idle_time=0;

	currTime=0.0;
	total_qlen_tchan_=NULL;
	qlen_tchan_=NULL;

	/* bind variables */
	bind("queue_num_", &queue_num_);
	bind("mean_pktsize_", &mean_pktsize_);
	bind("port_thresh_",&port_thresh_);
	bind("marking_scheme_",&marking_scheme_);
	bind("estimate_weight_alpha_",&estimate_weight_alpha_);
	bind("estimate_weight_interval_bytes_",&estimate_weight_interval_bytes_);
	bind_bool("estimate_weight_enable_timer_",&estimate_weight_enable_timer_);
	bind_bw("link_capacity_",&link_capacity_);
	bind_bool("debug_",&debug_);

	/* Initialize queue states */
	qs=new QueueState[WFQ_MAX_QUEUES];
	for(int i=0;i<WFQ_MAX_QUEUES;i++)
	{
		qs[i].q_=new PacketQueue;
		qs[i].weight=10000.0;	//By default, weight is 10000
		qs[i].thresh=0.0;
	}
}

WFQ::~WFQ()
{
	for(int i=0;i<WFQ_MAX_QUEUES;i++)
	{
		delete[] qs[i].q_;
	}
	delete[] qs;
	timer_.cancel();
}

void WFQ::timeout(int)
{
	/* update weight_sum_estimate */
	weight_sum_estimate=weight_sum_estimate*estimate_weight_alpha_+weight_sum*(1-estimate_weight_alpha_);

	if(debug_&&marking_scheme_==MQ_MARKING_GENER)
		printf("%.9f smooth weight sum: %f, sample weight sum: %f\n",Scheduler::instance().clock(),weight_sum_estimate, weight_sum);

	/* reschedule timer */
	if(link_capacity_>0)
		timer_.resched(estimate_weight_interval_bytes_*8/link_capacity_);
}

void WFQ_Timer::expire(Event* e)
{
	queue_->timeout(0);
}

/* Get total length of all queues in bytes */
int WFQ::TotalByteLength()
{
	int result=0;
	for(int i=0;i<queue_num_;i++)
	{
		result+=qs[i].q_->byteLength();
	}
	return result;
}

/* Determine whether we need to mark ECN where q is current queue number. Return 1 if it requires marking */
int WFQ::MarkingECN(int q)
{
	if(q<0||q>=queue_num_)
	{
		fprintf (stderr,"illegal queue number\n");
		exit (1);
	}

	/* Per-queue ECN marking */
	if(marking_scheme_==PER_QUEUE_MARKING)
	{
		if(qs[q].q_->byteLength()>qs[q].thresh*mean_pktsize_)
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
	/* MQ-ECN for any packet scheduling algorithms */
	else if(marking_scheme_==MQ_MARKING_GENER)
	{
		double thresh=0;
		if(weight_sum_estimate>=0.000000001)
			thresh=min(qs[q].weight/weight_sum_estimate,1)*port_thresh_;
		else
			thresh=port_thresh_;

		if(qs[q].q_->byteLength()>thresh*mean_pktsize_)
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
 *   - $q set-weight queue_id queue_weight
 *   - $q set-thresh queue_id queue_thresh
 *   - $q attach-total file
 *	  - $q attach-queue file
 *
 *  NOTE: $q represents the discipline queue variable in OTcl.
 */
int WFQ::command(int argc, const char*const* argv)
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
				tcl.resultf("WFQ: trace: can't attach %s for writing", id);
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
				tcl.resultf("WFQ: trace: can't attach %s for writing", id);
				return (TCL_ERROR);
			}
			return (TCL_OK);
		}
	}
	else if (argc == 4)
	{
		if (strcmp(argv[1], "set-weight")==0)
		{
			int queue_id=atoi(argv[2]);
			if(queue_id<min(queue_num_,WFQ_MAX_QUEUES)&&queue_id>=0)
			{
				double weight=atof(argv[3]);
				if(weight>0)
				{
					qs[queue_id].weight=weight;
					return (TCL_OK);
				}
				else
				{
					fprintf (stderr,"illegal weight value %s for queue %s\n", argv[3], argv[2]);
					exit (1);
				}
			}
			/* Exceed the maximum queue number or smaller than 0*/
			else
			{
				fprintf (stderr,"no such queue\n");
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
					qs[queue_id].thresh=thresh;
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
				fprintf (stderr,"no such queue\n");
				exit (1);
			}
		}
	}
	return (Queue::command(argc, argv));
}

/* Receive a new packet */
void WFQ::enque(Packet *p)
{
	hdr_ip *iph=hdr_ip::access(p);
	int prio=iph->prio();
	hdr_flags* hf=hdr_flags::access(p);
	int pktSize=hdr_cmn::access(p)->size();
	int qlimBytes=qlim_*mean_pktsize_;
	queue_num_=min(queue_num_,WFQ_MAX_QUEUES);

	if(init==0)
	{
		/* Start timer*/
		if(marking_scheme_==MQ_MARKING_GENER && estimate_weight_enable_timer_ && estimate_weight_interval_bytes_>0 && link_capacity_>0)
			timer_.resched(estimate_weight_interval_bytes_*8/link_capacity_);
		init=1;
	}

	if(TotalByteLength()==0 && marking_scheme_==MQ_MARKING_GENER && !estimate_weight_enable_timer_)
	{
		double now=Scheduler::instance().clock();
		double idleTime=now-last_idle_time;
		if(estimate_weight_interval_bytes_>0 && link_capacity_>0)
			weight_sum_estimate=weight_sum_estimate*pow(estimate_weight_alpha_,idleTime/(estimate_weight_interval_bytes_*8/link_capacity_));
		else
			weight_sum_estimate=0;
		last_update_time=now;

		if(debug_)
			printf("%.9f smooth weight sum is reset to %f\n",now,weight_sum_estimate);
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

	/* If queue for the flow is empty, calculate headFinishTime and currTime */
	if(qs[prio].q_->length()==0)
	{
		weight_sum+=qs[prio].weight;
		if(qs[prio].weight>0)
		{
			qs[prio].headFinishTime=currTime+pktSize/qs[prio].weight ;
			currTime=qs[prio].headFinishTime;
		}
		/* In theory, weight should never be zero or negative */
		else
		{
			fprintf (stderr,"enqueue: illegal weight value for queue %d\n", prio);
			exit(1);
		}
	}

	/* Enqueue ECN marking */
	qs[prio].q_->enque(p);
	if(MarkingECN(prio)>0&&hf->ect())
		hf->ce() = 1;

	trace_qlen();
	trace_total_qlen();
}

Packet *WFQ::deque(void)
{
	Packet *pkt=NULL, *nextPkt=NULL;
	int i, pktSize;
	long double minT=LDBL_MAX ;
	int queue=-1;
	int tot_len=TotalByteLength();

	/* Switch port is not empty */
	if(tot_len>0)
	{
		/* look for the candidate queue with the earliest virtual finish time */
		for(i=0; i<queue_num_; i++)
		{
			if (qs[i].q_->length()==0)
				continue;

			if (qs[i].headFinishTime<minT)
			{
				queue=i;
				minT=qs[i].headFinishTime;
			}
		}

		if(queue==-1 && tot_len>0)
		{
			fprintf (stderr,"not work conserving\n");
			exit(1);
		}

		pkt=qs[queue].q_->deque();
		pktSize=hdr_cmn::access(pkt)->size();
		/* Set the headFinishTime for the remaining head packet in the queue */
		nextPkt=qs[queue].q_->head();
		if(nextPkt!=NULL)
		{
			if(qs[queue].weight>0)
			{
				qs[queue].headFinishTime=qs[queue].headFinishTime+(hdr_cmn::access(nextPkt)->size())/qs[queue].weight;
				if(currTime<qs[queue].headFinishTime)
					currTime=qs[queue].headFinishTime;
			}
			else
			{
				fprintf (stderr,"dequeue: illegal weight value\n");
				exit(1);
			}
		}
		/* After dequeue, the queue becomes empty */
		else
		{
			weight_sum-=qs[queue].weight;
			qs[queue].headFinishTime=LDBL_MAX;
		}
	}

	if(marking_scheme_==MQ_MARKING_GENER && !estimate_weight_enable_timer_)
	{
		double now=Scheduler::instance().clock();
		double timeInterval=now-last_update_time;
		if(estimate_weight_interval_bytes_>0 && link_capacity_>0 && timeInterval>=0.995*estimate_weight_interval_bytes_*8/link_capacity_)
		{
			weight_sum_estimate=weight_sum_estimate*estimate_weight_alpha_+weight_sum*(1-estimate_weight_alpha_);
			last_update_time=now;
			if(debug_)
				printf("%.9f smooth weight sum: %f, sample weight sum: %f\n",now, weight_sum_estimate, weight_sum);
		}

		if(TotalByteLength()==0)
			last_idle_time=now;
	}

	return pkt;
}

/* routine to write total qlen records */
void WFQ::trace_total_qlen()
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
void WFQ::trace_qlen()
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

		for(int i=0;i<min(queue_num_,WFQ_MAX_QUEUES); i++)
		{
			sprintf(wrk, ", %d",qs[i].q_->byteLength());
			n=strlen(wrk);
			wrk[n]=0;
			(void)Tcl_Write(qlen_tchan_, wrk, n);
		}
		(void)Tcl_Write(qlen_tchan_, "\n", 1);
	}
}
