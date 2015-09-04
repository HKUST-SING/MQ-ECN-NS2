#ifndef ns_wfq_h
#define ns_wfq_h

#include "queue.h"
#include "config.h"
#include "trace.h"
#include "timer-handler.h"

#include <iostream>
#include <queue>
using namespace std;

/*We can use 32 queuess at most */
#define WFQ_MAX_QUEUES 32

/* Per-queue ECN marking */
#define PER_QUEUE_MARKING 0
/* Per-port ECN marking */
#define PER_PORT_MARKING 1
/* MQ-ECN for any packet scheduling algorithms */
#define MQ_MARKING_GENER 2

class WFQ;

class WFQ_Timer : public TimerHandler
{
public:
	WFQ_Timer(WFQ *q) : TimerHandler() { queue_=q;}

protected:
	virtual void expire(Event *e);
	WFQ *queue_;
};

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
		void timeout(int);
		virtual int command(int argc, const char*const* argv);

	protected:
		Packet *deque(void);
		void enque(Packet *pkt);

		int TotalByteLength();	// Get total length of all queues in bytes
		int MarkingECN(int q); // Determine whether we need to mark ECN, q is current queue number

		/* Variables */
		struct QueueState *qs;	// Underlying multi-FIFO (CoS) queues and their states
		long double currTime;	//Finish time assigned to last packet
		WFQ_Timer timer_;	//Underlying timer for weight_sum_estimate update
		double weight_sum_estimate;	//estimation value for sum of weights of all non-empty  queues
		double weight_sum;	//sum of weights of all non-empty queues
		double last_update_time;	//last time when we update quantum_sum_estimate
		double last_idle_time;	//Last time when link becomes idle
		int init;	//whether the timer has been started

		int queue_num_;	// Number of queues
		int mean_pktsize_;	// MTU in bytes
		double port_thresh_;	// Per-port ECN marking threshold (pkts)
		int marking_scheme_;	// ECN marking policy
		double estimate_weight_alpha_;	//factor between 0 and 1 for weight estimation
		double estimate_weight_interval_bytes_;	//Time interval is estimate_weight_interval_bytes_/link capacity.
		int estimate_weight_enable_timer_;	//Whether we use real timer (TimerHandler) for weight estimation
		double link_capacity_;	//Link capacity
		int debug_;	//debug more(true) or not(false)

		Tcl_Channel total_qlen_tchan_;	// Place to write total_qlen records
		Tcl_Channel qlen_tchan_;	// Place to write per-queue qlen records
		void trace_total_qlen();	// Routine to write total qlen records
		void trace_qlen();	// Routine to write per-queue qlen records
};

#endif
