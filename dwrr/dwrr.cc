#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "flags.h"
#include "dwrr.h"

#define max(arg1,arg2) (arg1>arg2 ? arg1 : arg2)
#define min(arg1,arg2) (arg1<arg2 ? arg1 : arg2)

void DWRR_Timer::expire(Event *e)
{
	queue->timeout(0);
}

/* Insert a queue to the tail of an active list. Return true if insert succeeds */
static void InsertTailList(PacketDWRR* list, PacketDWRR *q)
{
	if(q!=NULL && list !=NULL)
	{
		PacketDWRR* tmp=list;
		while(true)
		{
			/* Arrive at the tail of this list */
			if(tmp->next==NULL)
			{
				tmp->next=q;
				q->next=NULL;
				return;
			}
			/* Move to next node */
			else
			{
				tmp=tmp->next;
			}
		}
	}
}

/* Remove and return the head node from the active list */
static PacketDWRR* RemoveHeadList(PacketDWRR* list)
{
	if(list!=NULL)
	{
		PacketDWRR* tmp=list->next;
		if(tmp!=NULL)
		{
			list->next=tmp->next;
			return tmp;
		}
		/* This list is empty */
		else
		{
			return NULL;
		}
	}
	else
	{
		return NULL;
	}
}

static class DWRRClass : public TclClass 
{
	public:
		DWRRClass() : TclClass("Queue/DWRR") {}
		TclObject* create(int argc, const char*const* argv) 
		{
			return (new DWRR);
		}
} class_dwrr;

DWRR::DWRR():timer(this),init(0)
{		
	queues=new PacketDWRR[MAX_QUEUE_NUM];
	activeList=new PacketDWRR();
	
	total_qlen_tchan_=NULL;
	qlen_tchan_=NULL;
	
	/* bind variables */
	bind("queue_num_", &queue_num_);
	bind("mean_pktsize_", &mean_pktsize_);
	bind("port_thresh_",&port_thresh_);
	bind("marking_scheme_",&marking_scheme_);
	bind("round_",&round_);
	bind("round_time_",&round_time_);
	bind_time("estimate_rate_period_",&estimate_rate_period_);
	bind("estimate_rate_alpha_",&estimate_rate_alpha_);
	bind("estimate_round_alpha_",&estimate_rate_alpha_);
}

DWRR::~DWRR() 
{
	delete activeList;
	delete [] queues;
	timer.cancel();
}

/* Get total length of all queues in bytes */
int DWRR::TotalByteLength()
{
	int result=0;
	for(int i=0;i<queue_num_;i++)
	{
		result+=queues[i].byteLength();
	}
	return result;
}

/* Determine whether we need to mark ECN where q is current queue number. Return 1 if it requires marking */
int DWRR::MarkingECN(int q)
{	
	if(q<0||q>=queue_num_)
	{
		fprintf (stderr,"illegal queue number\n");
		exit (1);
	}
	
	/* Per-queue ECN marking */
	if(marking_scheme_==PER_QUEUE_MARKING)
	{
		if(queues[q].byteLength()>=queues[q].thresh*mean_pktsize_)
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
		if((queues[q].byteLength()>=queues[q].thresh*mean_pktsize_&&marking_scheme_==QUEUE_SMART_MARKING)||
		(queues[q].byteLength()>=queues[q].thresh*mean_pktsize_&&TotalByteLength()>=port_thresh_*mean_pktsize_&&marking_scheme_==HYBRID_SMRT_MARKING))
		{
			int backlogged_quantum_sum=0;	//Sum of quantum of backlogged queues
			double thresh=0;
		
			/* Find all backlogged queues and get the sum of their quantum */
			for(int i=0;i<queue_num_;i++)
			{
				if(queues[i].backlogged)	
					backlogged_quantum_sum+=queues[i].quantum;
			}
	
			if(backlogged_quantum_sum>0)
			{
				thresh=queues[q].quantum*port_thresh_/backlogged_quantum_sum;
				if(queues[q].byteLength()>=thresh*mean_pktsize_)
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

/* Timeout callback function */
void DWRR::timeout(int)
{
	for(int i=0;i<queue_num_;i++)
	{
		double rate=queues[i].input_bytes/estimate_rate_period_;
		queues[i].input_bytes=0;
		queues[i].input_rate=queues[i].input_rate*estimate_rate_alpha_+(1-estimate_rate_alpha_)*rate;
		//For debug
		//fprintf(stderr, "%f ", queues[i].input_rate);
	}
	//fprintf(stderr, "\n");
	timer.resched(estimate_rate_period_);	//Restart timer
	
	//For debug
	//double now=Scheduler::instance().clock();
	//fprintf(stderr,"%f %f\n",now,estimate_period_);
}

/* Receive a new packet */
void DWRR::enque(Packet *p)
{
	hdr_ip *iph=hdr_ip::access(p);
	int prio=iph->prio();
	hdr_flags* hf=hdr_flags::access(p);
	int pktSize=hdr_cmn::access(p)->size();
	int qlimBytes=qlim_*mean_pktsize_;
	
	if(!init)
	{
		/* Start timer */
		timer.resched(estimate_rate_period_);
		queue_num_=min(queue_num_,MAX_QUEUE_NUM);
		init=1;
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
	queues[prio].input_bytes+=pktSize;
	
	/* Backlogged judge: queue length+(round-1)*T*input_rate > Deficit Counter + round*quantum */
	if(queues[prio].current==false&&queues[prio].byteLength()+(round_-1)*round_time_*queues[prio].input_rate>queues[prio].deficitCounter+round_*queues[prio].quantum
	||queues[prio].current==true&&queues[prio].byteLength()+(round_-1)*round_time_*queues[prio].input_rate>queues[prio].deficitCounter+(round_-1)*queues[prio].quantum)
		queues[prio].backlogged=true;
	else
		queues[prio].backlogged=false;
	
	/* Enqueue ECN marking */
	if(queues[prio].backlogged&&MarkingECN(prio)>0&&hf->ect())	
		hf->ce() = 1;
	
	//For debug
	//printf ("enque to %d\n", prio);
	
	/* if queues[prio] is not in activeList */
	if(queues[prio].active==false)
	{
		queues[prio].deficitCounter=0;
		queues[prio].active=true;
		queues[prio].current=false;
		queues[prio].start_time=Scheduler::instance().clock();	//Start time of this round
		InsertTailList(activeList, &queues[prio]);
		//printf("Insert to activeList\n");
	}
	
	trace_qlen();
	trace_total_qlen();
}

Packet *DWRR::deque(void)
{
	PacketDWRR *headNode=NULL;
	Packet *pkt=NULL;
	Packet *headPkt=NULL;
	int pktSize;
	
	/*At least one queue is active, activeList is not empty */
	if(TotalByteLength()>0)
	{
		/* We must go through all actives queues and select a packet to dequeue */
		while(1)
		{
			headNode=activeList->next;	//Get head node from activeList					
			if(headNode==NULL)
				fprintf (stderr,"no active flow\n");
			
			/* if headNode is not empty */
			if(headNode->length()>0)
			{
				/* headNode has not been served yet in this round */
				if(headNode->current==false)
				{
					headNode->deficitCounter+=headNode->quantum;
					headNode->current=true;
				}
				headPkt=headNode->head();
				pktSize=hdr_cmn::access(headPkt)->size();
				
				/* if we have enough quantum to dequeue the head packet */
				if(pktSize<=headNode->deficitCounter)
				{
					pkt=headNode->deque();
					headNode->deficitCounter-=pktSize;
					//printf("deque a packet\n");
					
					/* After dequeue, headNode becomes empty queue */
					if(headNode->length()==0)
					{
						headNode=RemoveHeadList(activeList);	//Remove head node from activeList
						headNode->deficitCounter=0;
						headNode->active=false;
						headNode->current=false;
						double round_time_sample=Scheduler::instance().clock()-headNode->start_time;
						//printf ("now %f start time %f\n", Scheduler::instance().clock(),headNode->start_time);
						round_time_=round_time_*estimate_round_alpha_+round_time_sample*(1-estimate_round_alpha_);
					}
					break;
				}
				/* if we don't have enough quantum to dequeue the head packet and the queue is not empty */
				else
				{
					headNode=RemoveHeadList(activeList);	
					headNode->current=false;
					double round_time_sample=Scheduler::instance().clock()-headNode->start_time;
					//printf ("now %f start time %f\n", Scheduler::instance().clock(),headNode->start_time);
					round_time_=round_time_*estimate_round_alpha_+round_time_sample*(1-estimate_round_alpha_);
					headNode->start_time=Scheduler::instance().clock();	//Reset start time 
					InsertTailList(activeList, headNode);
				}
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