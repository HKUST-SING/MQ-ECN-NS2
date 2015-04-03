#ifndef ns_wf2q_h
#define ns_wf2q_h

#include "queue.h"
#include "config.h"

/*We can use 8 queuess at most */
#define WF2Q_MAX_QUEUES 8	

/* Per-queue ECN marking */
#define PER_QUEUE_MARKING 0
/* Per-port ECN marking */
#define PER_PORT_MARKING 1
/* Our smart hybrid ECN marking scheme */
#define SMART_MARKING 2

/* When buffer occupation of a queue is no smaller than than QUEUE_MIN_BYTES, we think it's a 'busy' queue */
#define QUEUE_MIN_BYTES 3000

struct QueueState
{
	PacketQueue* q_;	/* packet queue associated to the corresponding service */
	double weight;	/* Weight of the service */
    long double S;	/* Starting time of the queue , not checked for wraparound*/
    long double F;	/* Ending time of the queue, not checked for wraparound */
	int thresh;	/* Per-queue ECN marking threshold (pkts)*/
};

class WF2Q : public Queue 
{
	public:
		WF2Q();
		~WF2Q();
		virtual int command(int argc, const char*const* argv);
		
	protected:
		Packet *deque(void);
		void enque(Packet *pkt);
		int TotalByteLength();	/* Get total length of all queues in bytes */
		int MarkingECN(int q); /* Determine whether we need to mark ECN, q is current queue number */
		
		/* Variables */
		struct QueueState *qs;	/* underlying multi-FIFO (CoS) queues and their states */
		long double V;	/* Virtual time , not checked for wraparound!*/
		int queue_num_;	/*Number of queues */
		int dequeue_ecn_marking_;	/* Enable dequeue ECN marking or not */
		int mean_pktsize_;	/* MTU in bytes */
		int port_thresh_;	/* Per-port ECN marking threshold (pkts)*/
		int marking_scheme_;	/* ECN marking policy */
};

#endif