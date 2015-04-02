#ifndef ns_wf2q_h
#define ns_wf2q_h

#include "queue.h"
#include "config.h"

/*We can use 8 queuess at most */
#define WF2Q_MAX_QUEUES 8	

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
		double WeightedThresh();	/* return ECN marking threshold for weight '1' */
		int NonEmptyQueues(int q);		/* return the number of non-empty queues except for queue q */ 
		
		/* Variables */
		struct QueueState *qs;	/* underlying multi-FIFO (CoS) queues and their states */
		long double V;	/* Virtual time , not checked for wraparound!*/
		int queue_num_;	/*Number of queues */
		int dequeue_ecn_marking_;	/* Enable dequeue ECN marking or not */
		int mean_pktsize_;	/* MTU in bytes */
		int port_thresh_;	/* Per-port ECN marking threshold (pkts)*/
			
		//int port_ecn_marking_;	/* Enable per-port ECN marking or not */
		//int port_low_thresh_;	/* The low per-port ECN marking threshold (pkts)*/
		//int port_high_thresh_;	/* The high per-port ECN marking threshold (pkts)*/
		//int queue_thresh_;	/* The per-queue ECN marking threshold (pkts)*/
};

#endif