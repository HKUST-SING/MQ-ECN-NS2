#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "flags.h"
#include "dwrr.h"

#define max(arg1,arg2) (arg1>arg2 ? arg1 : arg2)
#define min(arg1,arg2) (arg1<arg2 ? arg1 : arg2)

static class DWRRClass : public TclClass 
{
	public:
		DWRRClass() : TclClass("Queue/DWRR") {}
		TclObject* create(int argc, const char*const* argv) 
		{
			return (new DWRR);
		}
} class_dwrr;

DWRR::DWRR()
{
	/* bind variables */
	bind("queue_num_", &queue_num_);
	bind("mean_pktsize_", &mean_pktsize_);
	bind("port_thresh_",&port_thresh_);
	bind("marking_scheme_",&marking_scheme_);
	bind("backlogged_in_bytes_",&backlogged_in_bytes_);
	
	turn=0;
	total_qlen_tchan_=NULL;
	qlen_tchan_=NULL;
	
	/* Initialize queue states */
	qs=new QueueState[DWRR_MAX_QUEUES];
	for(int i=0;i<DWRR_MAX_QUEUES;i++)
	{
		qs[i].q_=new PacketQueue;
		//By default, quantum is 100
		qs[i].quantum=100;
		qs[i].deficitCounter=0;
		qs[i].thresh=0.0;
	}
}

DWRR::~DWRR() 
{
	for(int i=0;i<DWRR_MAX_QUEUES;i++)
	{
		delete[] qs[i].q_;
	}
	delete[] qs;
}

/* Get total length of all queues in bytes */
int DWRR::TotalByteLength()
{
	int result=0;
	for(int i=0;i<DWRR_MAX_QUEUES;i++)
	{
		result+=qs[i].q_->byteLength();
	}
	return result;
}

/* Determine whether we need to mark ECN where q is current queue number. Return 1 if it requires marking */
int DWRR::MarkingECN(int q)
{	
	if(q<0||q>=min(queue_num_,DWRR_MAX_QUEUES))
	{
		fprintf (stderr,"illegal queue number\n");
		exit (1);
	}
	
	/* Per-queue ECN marking */
	if(marking_scheme_==PER_QUEUE_MARKING)
	{
		if(qs[q].q_->byteLength()>=qs[q].thresh*mean_pktsize_)
			return 1;
		else
			return 0;
	}
	/* Per-port ECN marking */
	else if(marking_scheme_==PER_PORT_MARKING)
	{
		if(TotalByteLength()>=port_thresh_*mean_pktsize_)
			return 1;
		else
			return 0;
	}
	/* Our new ECN marking schemes (QUEUE_SMART_MARKING or HYBRID_SMRT_MARKING)*/
	else if(marking_scheme_==QUEUE_SMART_MARKING||marking_scheme_==HYBRID_SMRT_MARKING)
	{	
		/* For QUEUE_SMART_MARKING, we calculate ECN thresholds when:
		 * 		1. per-queue buffer occupation exceeds a pre-defined threshold 
		 *  For HYBRID_SMRT_MARKING, we calculate ECN thresholds when:
		 *			1. per-port buffer occupation exceeds a pre-defined threshold for 
		 *			2. per-queue buffer occupation also exceeds a pre-defined threshold
		 */ 
		if((qs[q].q_->byteLength()>=qs[q].thresh*mean_pktsize_&&marking_scheme_==QUEUE_SMART_MARKING)||
		(qs[q].q_->byteLength()>=qs[q].thresh*mean_pktsize_&&TotalByteLength()>=port_thresh_*mean_pktsize_&&marking_scheme_==HYBRID_SMRT_MARKING))
		{
			int quantum_sum=0;
			double thresh=0;
		
			/* Find all backlogged queues and get the sum of their quantum */
			for(int i=0;i<min(queue_num_,DWRR_MAX_QUEUES);i++)
			{
				/* Determine whether a queue is backlogged: 
				 * if backlogged_in_bytes_ is set, qlen_in_bytes>=2*MTU
				 * otherwise, qlen_in_pkts>=2
				 */
				if((backlogged_in_bytes_==1&&qs[i].q_->byteLength()>=2*mean_pktsize_)||
				(backlogged_in_bytes_==0&&qs[i].q_->length()>=2))
					quantum_sum+=qs[i].quantum;
			}
	
			if(quantum_sum>0)
			{
				thresh=qs[q].quantum*port_thresh_/quantum_sum;
				if(qs[q].q_->byteLength()>=thresh*mean_pktsize_)
					return 1;
				else 
					return 0;
			}
			else
			{
				return 0;
			}
		}
		else
		{
			return 0;
		}
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
int DWRR::command(int argc, const char*const* argv)
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
				tcl.resultf("DWRR: trace: can't attach %s for writing", id);
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
				tcl.resultf("DWRR: trace: can't attach %s for writing", id);
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
			if(queue_id<min(queue_num_,DWRR_MAX_QUEUES)&&queue_id>=0)
			{
				int quantum=atoi(argv[3]);
				if(quantum>0)
				{
					qs[queue_id].quantum=quantum;
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
			if(queue_id<min(queue_num_,DWRR_MAX_QUEUES)&&queue_id>=0)
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
				fprintf (stderr,"no such queue %s\n",argv[2]);
				exit (1);
			}
		}
	}
	return (Queue::command(argc, argv));	
}

/* Receive a new packet */
void DWRR::enque(Packet *p)
{
	hdr_ip *iph=hdr_ip::access(p);
	int prio=iph->prio();
	hdr_flags* hf=hdr_flags::access(p);
	int pktSize=hdr_cmn::access(p)->size();
	int qlimBytes=qlim_*mean_pktsize_;
		
	/* The shared buffer is overfilld */
	if(TotalByteLength()+pktSize>qlimBytes)
	{
		drop(p);
		//printf("Packet drop\n");
		return;
	}
	
	if(prio>=min(queue_num_,DWRR_MAX_QUEUES)&&prio>0)
		prio=min(queue_num_,DWRR_MAX_QUEUES)-1;
	
	/* Enqueue ECN marking */
	if(MarkingECN(prio)>0&&hf->ect())
		hf->ce() = 1;
	
	//For debug
	//printf ("enque to %d\n", prio);
	qs[prio].q_->enque(p); 		
	trace_qlen();
	trace_total_qlen();
}

Packet *DWRR::deque(void)
{
	Packet *pkt=NULL;
	Packet *headPkt=NULL;
	int pktSize;
	
	/*At least one queue is active */
	if(TotalByteLength()>0)
	{
		/* Given buffer is not empty, we must go through all actives queues and select a packet to dequeue */
		while(1)
		{
			/* if queue[turn] is not empty */
			if(qs[turn].q_->length()>0)
			{
				qs[turn].deficitCounter+=qs[turn].quantum;
				headPkt=qs[turn].q_->head();
				pktSize=hdr_cmn::access(headPkt)->size();
				/* if we have enough quantum to dequeue the head packet */
				if(pktSize<=qs[turn].deficitCounter)
				{
					pkt=qs[turn].q_->deque();
					qs[turn].deficitCounter-=pktSize;
					/* After dequeue, queue[turn] becomes empty */
					if(qs[turn].q_->length()==0)
					{
						qs[turn].deficitCounter=0;
						turn=(turn+1)%min(queue_num_,DWRR_MAX_QUEUES);
					}
					break;
				}
				/* if we don't have enough quantum to dequeue packet from this queue, move to next queue */
				else
				{
					turn=(turn+1)%min(queue_num_,DWRR_MAX_QUEUES);
				}
			}
			/* if queue[turn] is empty, we should move to next queue */
			else
			{
				turn=(turn+1)%min(queue_num_,DWRR_MAX_QUEUES);
			}
		}
	}
	
	return pkt;
}

/* routine to write total qlen records */
void DWRR::trace_total_qlen()
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
void DWRR::trace_qlen()
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
		
		for(int i=0;i<min(queue_num_,DWRR_MAX_QUEUES); i++)
		{
			sprintf(wrk, ", %d",qs[i].q_->byteLength());
			n=strlen(wrk);
			wrk[n]=0; 
			(void)Tcl_Write(qlen_tchan_, wrk, n);
		}
		(void)Tcl_Write(qlen_tchan_, "\n", 1);
	}
}