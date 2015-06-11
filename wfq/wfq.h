#ifndef ns_wfq_h
#define ns_wfq_h

#include "queue.h"
#include "config.h"
#include "trace.h"

#include <iostream>       
#include <queue> 
using namespace std;

/*We can use 32 queuess at most */
#define WFQ_MAX_QUEUES 32	

/* Per-queue ECN marking */
#define PER_QUEUE_MARKING 0
/* Per-port ECN marking */
#define PER_PORT_MARKING 1
/* Our dynamic per-queue-based ECN marking scheme */
#define QUEUE_SMART_MARKING 2
/* Our dynamic hybrid (per-queue+per-port) ECN marking scheme */
#define HYBRID_SMRT_MARKING 3

struct QueueState
{
	PacketQueue* q_;	// Packet queue associated to the corresponding service 
	double weight;	// Weight of the service 
    long double headFinishTime;	// Finish time of the packet at head of this queue.
	double thresh;	// Per-queue ECN marking threshold (pkts)
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
		
		int TotalByteLength();	// Get total length of all queues in bytes 
		int MarkingECN(int q); // Determine whether we need to mark ECN, q is current queue number 
		
		/* Variables */
		struct QueueState *qs;	// Underlying multi-FIFO (CoS) queues and their states 
		long double currTime;	//Finish time assigned to last packet
		
		int queue_num_;	// Number of queues 
		int mean_pktsize_;	// MTU in bytes 
		double port_thresh_;	// Per-port ECN marking threshold (pkts)
		int marking_scheme_;	// ECN marking policy 
		int backlogged_in_bytes_;	// Backlogged conditions: >=two pkts (false) or two mean_pktsize_ (true)  
		
		Tcl_Channel total_qlen_tchan_;	// Place to write total_qlen records 
		Tcl_Channel qlen_tchan_;	// Place to write per-queue qlen records 
		void trace_total_qlen();	// Routine to write total qlen records 
		void trace_qlen();	// Routine to write per-queue qlen records 
};

#endif