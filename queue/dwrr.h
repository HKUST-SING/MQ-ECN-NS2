#ifndef ns_dwrr_h
#define ns_dwrr_h

#include "queue.h"
#include "config.h"
#include "trace.h"

/*Maximum queue number */
#define MAX_QUEUE_NUM 64

/* Per-queue ECN marking */
#define PER_QUEUE_MARKING 0
/* Per-port ECN marking */
#define PER_PORT_MARKING 1
/* MQ-ECN */
#define MQ_MARKING 2

class PacketDWRR;
class DWRR;

class PacketDWRR: public PacketQueue
{
	public:
		PacketDWRR(): quantum(1500), deficitCounter(0), thresh(0), active(false), current(false), start_time(0), next(NULL) {}

		int id;	//queue ID
		int quantum;	//quantum (weight) of this queue
		int deficitCounter;	//deficit counter for this queue
		double thresh;	//per-queue ECN marking threshold (pkts)
		bool active;	//whether this queue is active (qlen>0)
		bool current;	//whether this queue is currently being served (deficitCounter has been updated for thie round)
		double start_time;	//time when this queue is inserted to active list
		PacketDWRR *next;	//pointer to next node

		friend class DWRR;
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

		int TotalByteLength();	//Get total length of all queues in bytes
		bool MarkingECN(int q);	//Determine whether we need to mark ECN, q is queue index
		void trace_total_qlen();	//routine to write total qlen records
		void trace_qlen();	//routine to write per-queue qlen records

		/* Variables */
		PacketDWRR *queues;	//underlying multi-FIFO (CoS) queues
		PacketDWRR *activeList;	//list for active queues
		double round_time;	//estimation value for round time
		double last_idle_time;	//Last time when link becomes idle

		Tcl_Channel total_qlen_tchan_;	//place to write total_qlen records
		Tcl_Channel qlen_tchan_;	//place to write per-queue qlen records

		int queue_num_;	//number of queues
		int mean_pktsize_;	//MTU in bytes
		double port_thresh_;	//per-port ECN marking threshold (pkts)
		int marking_scheme_;	//ECN marking policy
		double estimate_round_alpha_;	//factor between 0 and 1 for round time estimation
		int estimate_round_idle_interval_bytes_;	//Time interval (divided by link capacity) to update round time when link is idle.
		double link_capacity_;	//Link capacity
		int deque_marking_;	//shall we enable dequeue ECN/RED marking
		int debug_;	//debug more(true) or not(false)
};

#endif
