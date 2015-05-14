#ifndef ns_wfq_h
#define ns_wfq_h

#include "queue.h"
#include "config.h"
#include "trace.h"

/*We can use 32 queuess at most */
#define WFQ_MAX_QUEUES 32	

/* Per-queue ECN marking */
#define PER_QUEUE_MARKING 0
/* Per-port ECN marking */
#define PER_PORT_MARKING 1
/* Our smart hybrid ECN marking scheme */
#define SMART_MARKING 2

struct QueueState
{
	PacketQueue* q_;	/* packet queue associated to the corresponding service */
	double weight;	/* Weight of the service */
    long double S;	/* Starting time of the queue , not checked for wraparound*/
    long double F;	/* Ending time of the queue, not checked for wraparound */
	int thresh;	/* Per-queue ECN marking threshold (pkts)*/
};

class WFQ : public Queue 
{
	public:
		WFQ();
		~WFQ();
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
		Tcl_Channel total_qlen_tchan_;	/* place to write total_qlen records */
		Tcl_Channel qlen_tchan_;	/* place to write per-queue qlen records */
		void trace_total_qlen();	/* routine to write total qlen records */
		void trace_qlen();	/* routine to write per-queue qlen records */
};

#endif