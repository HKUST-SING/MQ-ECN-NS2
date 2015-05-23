#ifndef ns_dwrr_h
#define ns_dwrr_h

#include "queue.h"
#include "config.h"
#include "trace.h"

/*We can use 32 queuess at most */
#define DWRR_MAX_QUEUES 32

/* Per-queue ECN marking */
#define PER_QUEUE_MARKING 0
/* Per-port ECN marking */
#define PER_PORT_MARKING 1
/* Dynamic per-queue ECN marking scheme */
#define QUEUE_SMART_MARKING 2
/* Dynamic hybrid (per-queue+per-port) ECN marking scheme */
#define HYBRID_SMRT_MARKING 3

struct QueueState
{
	PacketQueue* q_;	/* packet queue associated to the corresponding service */
	int quantum ;	/* quantum (weight) of this queue */
	int deficitCounter; /*deficit counter for this queue */
	double thresh;	/* per-queue ECN marking threshold (pkts)*/
};	

class DWRR : public Queue 
{
	public:
		DWRR();
		~DWRR();
		virtual int command(int argc, const char*const* argv);
		
	protected:
		Packet *deque(void);
		void enque(Packet *pkt);
		int TotalByteLength();	/* Get total length of all queues in bytes */
		int MarkingECN(int q); /* Determine whether we need to mark ECN, q is current queue number */
		
		/* Variables */
		struct QueueState *qs;	/* underlying multi-FIFO (CoS) queues and their states */
		int turn;	/* round-robin turn */
		
		int queue_num_;	/*number of queues */
		int mean_pktsize_;	/* MTU in bytes */
		double port_thresh_;	/* per-port ECN marking threshold (pkts)*/
		int marking_scheme_;	/* ECN marking policy */
		int backlogged_in_bytes_;	/* Backlogged conditions: >=two pkts or two mean_pktsize_  */
		
		Tcl_Channel total_qlen_tchan_;	/* place to write total_qlen records */
		Tcl_Channel qlen_tchan_;	/* place to write per-queue qlen records */
		void trace_total_qlen();	/* routine to write total qlen records */
		void trace_qlen();	/* routine to write per-queue qlen records */
};

#endif