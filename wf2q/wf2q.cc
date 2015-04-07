#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "flags.h"
#include "wf2q.h"

#define max(arg1,arg2) (arg1>arg2 ? arg1 : arg2)
#define min(arg1,arg2) (arg1<arg2 ? arg1 : arg2)

static class WF2QClass : public TclClass 
{
	public:
		WF2QClass() : TclClass("Queue/WF2Q") {}
		TclObject* create(int argc, const char*const* argv) 
		{
			return (new WF2Q);
		}
} class_wf2q;

WF2Q::WF2Q()
{
	/* bind variables */
	bind("queue_num_", &queue_num_);
	bind("dequeue_ecn_marking_", &dequeue_ecn_marking_);
	bind("mean_pktsize_", &mean_pktsize_);
	bind("port_thresh_",&port_thresh_);
	bind("marking_scheme_",&marking_scheme_);

	V=0.0;
	
	/* Initialize queue states */
	qs=new QueueState[WF2Q_MAX_QUEUES];
	for(int i=0;i<WF2Q_MAX_QUEUES;i++)
	{
		qs[i].q_=new PacketQueue;
		//By default, weight is 1000
		qs[i].weight=1000.0;
		qs[i].S=0.0;
		qs[i].F=0.0;
		qs[i].thresh=0;
	}
}

WF2Q::~WF2Q() 
{
	for(int i=0;i<WF2Q_MAX_QUEUES;i++)
	{
		delete[] qs[i].q_;
	}
	delete[] qs;
}

/* Get total length of all queues in bytes */
int WF2Q::TotalByteLength()
{
	int result=0;
	for(int i=0;i<WF2Q_MAX_QUEUES;i++)
	{
		result+=qs[i].q_->byteLength();
	}
	return result;
}

/* Determine whether we need to mark ECN where q is current queue number. Return 1 if it requires marking */
int WF2Q::MarkingECN(int q)
{	
	if(q<0||q>=min(queue_num_,WF2Q_MAX_QUEUES))
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
	/* Our smart hybrid ECN marking scheme */
	else if(marking_scheme_==SMART_MARKING)
	{	
		/* We only do ECN marking when per-queue buffer occupation exceeds the pre-defined threshold */ 
		if(qs[q].q_->byteLength()>=qs[q].thresh*mean_pktsize_)//&&TotalByteLength()>=port_thresh_*mean_pktsize_)
		{
			double weights=0;
			double thresh=0;
		
			/* Find all 'busy' queues and get the sum of their weights */
			for(int i=0;i<min(queue_num_,WF2Q_MAX_QUEUES);i++)
			{
				if(qs[i].q_->byteLength()>=QUEUE_MIN_BYTES)
					weights+=qs[i].weight;
			}
	
			if(int(weights)>0)
			{
				thresh=qs[q].weight*port_thresh_/weights;
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
 *   - $q set-weight queue_id queue_weight
 *   - $q set-thresh queue_id queue_thresh
 *   - $q attach file
 *
 *  NOTE: $q represents the discipline queue variable in OTcl.
 */
int WF2Q::command(int argc, const char*const* argv)
{
	if(argc==3)
	{
		// attach a file for variable tracing
		if (strcmp(argv[1], "attach") == 0) 
		{
			int mode;
			const char* id = argv[2];
			Tcl& tcl = Tcl::instance();
			tchan_=Tcl_GetChannel(tcl.interp(), (char*)id, &mode);
			if (tchan_==0) 
			{
				tcl.resultf("WF2Q: trace: can't attach %s for writing", id);
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
			if(queue_id<min(queue_num_,WF2Q_MAX_QUEUES)&&queue_id>=0)
			{
				double weight=atof(argv[3]);
				if(int(weight)>0)
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
			if(queue_id<min(queue_num_,WF2Q_MAX_QUEUES)&&queue_id>=0)
			{
				int thresh=atoi(argv[3]);
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
void WF2Q::enque(Packet *p)
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
	
	if(prio>=min(queue_num_,WF2Q_MAX_QUEUES)&&prio>0)
		prio=min(queue_num_,WF2Q_MAX_QUEUES)-1;
	
	//For debug
	//printf ("enque to %d\n", prio);
	
	/* If queue for the flow is empty, calculate start and finish times */
	if(qs[prio].q_->length()==0)
	{
		qs[prio].S=max(V, qs[prio].F);
		if(int(qs[prio].weight)>0)
		{
			qs[prio].F=qs[prio].S+pktSize/qs[prio].weight;
		}
		/* In theory, weight should never be zero or negative */
		else
		{
			fprintf (stderr,"enqueue: illegal weight value for queue %d\n", prio);
			exit(1);
		}
		
		/* update system virutal clock */
		long double minS = qs[prio].S;
		for(int i=0;i<min(queue_num_,WF2Q_MAX_QUEUES);i++)
		{
			if(qs[i].q_->length()>0&&minS>qs[i].S)
				minS=qs[i].S;
		}
		V=max(minS, V);
	}
	
	/* Enqueue ECN marking */
	if(dequeue_ecn_marking_==0)
	{
		if(MarkingECN(prio)>0&&hf->ect())
			hf->ce() = 1;
	}
	qs[prio].q_->enque(p); 		
	trace();
}

Packet *WF2Q::deque(void)
{
	Packet *pkt=NULL, *nextPkt=NULL;
	int i, pktSize;
	long double minF=LDBL_MAX ;
	int queue=-1;
	double weight=0.0;
	
	/* look for the candidate queue with the earliest finish time */
	for(i=0; i<min(queue_num_,WF2Q_MAX_QUEUES); i++)
	{
		if (qs[i].q_->length()==0)
			continue;
		if (qs[i].S<=V && qs[i].F<minF)
		{
			queue=i;
			minF=qs[i].F;
		}
	}
	
	if (queue==-1)
		return NULL;
	
	//For debug
	//printf ("deque from %d\n", queue);
	
	pkt=qs[queue].q_->deque();
	pktSize=hdr_cmn::access(pkt)->size();
	  
	/* Set the start and the finish times of the remaining packets in the queue */
	nextPkt=qs[queue].q_->head();
	if (nextPkt!=NULL) 
	{
		qs[queue].S=qs[queue].F;
		if(int(qs[queue].weight)>0)
		{
			qs[queue].S=qs[queue].F;
			qs[queue].F=qs[queue].S+(hdr_cmn::access(nextPkt)->size())/qs[queue].weight;
		}
		else
		{
			fprintf (stderr,"dequeue: illegal weight value\n");
			exit(1);
		}
	}
	
	/* update the virtual clock */
	long double minS = qs[queue].S;
	for(int i=0;i<min(queue_num_,WF2Q_MAX_QUEUES);i++)
	{
		weight+=qs[i].weight;
		if(qs[i].q_->length()>0&&minS>qs[i].S)
			minS=qs[i].S;
	}
	if(int(weight)>0)
		V=max(minS, V+pktSize/weight);
	
	/* Dequeue ECN marking */
	if(pkt!=NULL&&dequeue_ecn_marking_!=0)
	{
		hdr_flags* hf=hdr_flags::access(pkt);		
		if(MarkingECN(queue)>0&&hf->ect()) 
		{
			hf->ce() = 1;
		}
	}
	
	trace();
	return pkt;
}

/* routine to write trace records */
void WF2Q::trace()
{
	if (tchan_) 
	{
		char wrk[500]={0};
		int n;
		double t = Scheduler::instance().clock();
		sprintf(wrk, "%g, %d", t,TotalByteLength());
		n=strlen(wrk);
		wrk[n] = '\n'; 
		wrk[n+1] = 0;
		(void)Tcl_Write(tchan_, wrk, n+1);
	}
}
