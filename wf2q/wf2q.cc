#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "flags.h"
#include "wf2q.h"

#define max(arg1,arg2) (arg1>arg2 ? arg1 : arg2)

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
	bind("port_ecn_marking_", &port_ecn_marking_);
	bind("mean_pktsize_", &mean_pktsize_);
	bind("port_low_thresh_", &port_low_thresh_);
	bind("port_high_thresh_", &port_high_thresh_);
	bind("queue_thresh_", &queue_thresh_);
	
	V=0.0;
	if(queue_num_<=0)
		queue_num_=WF2Q_QUEUES;
	
	/* Initialize queue states */
	qs=new QueueState[queue_num_];
	for(int i=0;i<queue_num_;i++)
	{
		qs[i].q_=new PacketQueue;
		/* By default, weight is 1000 for each queue given that MTU is 1500 bytes */
		qs[i].weight=1000.0;
		qs[i].S=0.0;
		qs[i].F=0.0;
	}
}

WF2Q::~WF2Q() 
{
	for(int i=0;i<queue_num_;i++)
	{
		delete[] qs[i].q_;
	}
	delete[] qs;
}

/* Get total length of all queues in bytes */
int WF2Q::TotalByteLength()
{
	int result=0;
	for(int i=0;i<queue_num_;i++)
	{
		result+=qs[i].q_->byteLength();
	}
	return result;
}

/* return ECN marking threshold for weight '1' */
int WF2Q::WeightedThresh()
{
	double bytes=0;
	double weights=0.0;
	
	for(int i=0;i<queue_num_;i++)
	{
		/* We only consider no-empty queues: more than 1 packet */
		if(qs[i].q_->length()>1)
		{
			bytes+=qs[i].weight*qs[i].q_->byteLength();
			weights+=qs[i].weight;
		}
	}
	
	if(weights>0)
		return int(bytes/weights);
	else
		return 0;
}

/* 
 *  entry points from OTcL to set per queue state variables
 *   - $q set-weight queue_id queue_weight
 *
 *  NOTE: $q represents the discipline queue variable in OTcl.
 */
int WF2Q::command(int argc, const char*const* argv)
{
	if (argc == 4) 
	{
		if (strcmp(argv[1], "set-weight\0") == 0) 
		{
			int queue_id=atoi(argv[2]);
			if(queue_id<queue_num_)
			{
				double weight = atof(argv[3]);
				if(weight>0)
				{
					qs[queue_id].weight=weight;
					return (TCL_OK);
				}
				/* Negative or zero weight */
				else
				{
					fprintf (stderr,"illegal weight value\n");
					exit (1);
				}
			}
			/* Exceed the maximum queue number */
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
		printf("Packet drop\n");
		return;
	}
	
	if(prio>=queue_num_)
		prio=queue_num_-1;
	
	//For debug
	//printf ("enque to %d\n", prio);
	
	/* If queue for the flow is empty, calculate start and finish times */
	if(qs[prio].q_->length()==0)
	{
		qs[prio].S=max(V, qs[prio].F);
		if(qs[prio].weight>0)
		{
			qs[prio].F=qs[prio].S+pktSize/qs[prio].weight;
		}
		/* In theory, weight should never be zero or negative */
		else
		{
			fprintf (stderr,"illegal weight value\n");
			exit(1);
		}
		
		/* update system virutal clock */
		long double minS = qs[prio].S;
		for(int i=0;i<queue_num_;i++)
		{
			if(qs[i].q_->length()>0&&minS>qs[i].S)
				minS=qs[i].S;
		}
		V=max(minS, V);
	}
	
	/* Per-queue ECN marking */
	if(port_ecn_marking_==0)
	{
		if(qs[prio].q_->byteLength()+pktSize>=queue_thresh_*mean_pktsize_)
		{	/*If this packet is ECN-capable (ECT) */
			if(hf->ect()) 
				hf->ce() = 1;
		}
	}
	/* Per-port ECN marking */
	else
	{
		/* Total buffer occupation is larger than low ECN marking threshold */ 
		if(TotalByteLength()+pktSize>=port_low_thresh_*mean_pktsize_)
		{
			/* Queue buffer occupation is larger than weighted ECN marking threshold for this queue
			 * or total buffer occupation is larger than high ECN marking threshold */ 
			if(qs[prio].q_->byteLength()+pktSize>=WeightedThresh()*qs[prio].weight||
				TotalByteLength()+pktSize>=port_high_thresh_*mean_pktsize_)
			{	
				if (hf->ect())
					hf->ce() = 1;
			}
		}
	}

	qs[prio].q_->enque(p); 		
}

Packet *WF2Q::deque(void)
{
	Packet *pkt=NULL, *nextPkt=NULL;
	int i, pktSize;
	long double minF=LDBL_MAX ;
	int queue=-1;
	double weight=0.0;
	
	/* look for the candidate queue with the earliest finish time */
	for(i=0; i<queue_num_; i++)
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
	
	/* Set the start and the finish times of the remaining packets in the queue */
	nextPkt=qs[queue].q_->head();
	if (nextPkt!=NULL) 
	{
		qs[queue].S=qs[queue].F;
		if(qs[queue].weight>0)
		{
			qs[queue].S=qs[queue].F;
			qs[queue].F=qs[queue].S+(hdr_cmn::access(nextPkt)->size())/qs[queue].weight;
		}
		else
		{
			fprintf (stderr,"illegal weight value\n");
			exit(1);
		}
	}
	
	/* update the virtual clock */
	long double minS = qs[queue].S;
	for(int i=0;i<queue_num_;i++)
	{
		weight+=qs[i].weight;
		if(qs[i].q_->length()>0&&minS>qs[i].S)
			minS=qs[i].S;
	}
	if(weight>0)
		V=max(minS, V+pktSize/weight);
	return pkt;
}

