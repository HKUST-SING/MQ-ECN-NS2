#ifndef ns_dwrr_h
#define ns_dwrr_h

#include "queue.h"
#include "config.h"
#include "trace.h"

/*Maximum queue number */
#define MAX_QUEUE_NUM 32

/* Per-queue ECN marking */
#define PER_QUEUE_MARKING 0
/* Per-port ECN marking */
#define PER_PORT_MARKING 1
/* MQ-ECN for any packet scheduling algorithms */
#define MQ_MARKING_GENER 2
/* MQ-ECN for round robin packet scheduling algorithms */
#define MQ_MARKING_RR 3

class PacketDWRR;
class DWRR;

class PacketDWRR: public PacketQueue
{
	public: 
		PacketDWRR(): quantum(1500),deficitCounter(0),thresh(0),active(false),current(false),start_time(0),next(NULL) {} 
		
		int quantum;	//quantum (weight) of this queue 
		int deficitCounter;	//deficit counter for this queue 
		double thresh;	// per-queue ECN marking threshold (pkts)
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
		int TotalLength();	//Get total length of all queues in packets
		int TotalQuantum();	//Get sum of quantum
		int MarkingECN(int q); //Determine whether we need to mark ECN, q is current queue number 
		
		/* Variables */
		PacketDWRR *queues;	//underlying multi-FIFO (CoS) queues
		PacketDWRR *activeList;	//list for active queues	
		double round_time;	//estimation value for round time 
		double quantum_sum_estimate;	//estimation value for sum of quantums of all non-empty  queues
		int quantum_sum;	//sum of quantums of all non-empty queues in activeList
		
		bool init;
		
		int queue_num_;	//number of queues 
		int mean_pktsize_;	//MTU in bytes 
		double port_thresh_;	//per-port ECN marking threshold (pkts)
		int marking_scheme_;	//ECN marking policy 
		double estimate_round_alpha_;	//factor between 0 and 1 for round time estimation
		double estimate_quantum_alpha_;	//factor between 0 and 1 for quantum estimation
		int estimate_round_filter_;	//filter some round time samples
		double link_capacity_;	//Link capacity
		int debug_;	//debug more(true) or not(false)
		
		Tcl_Channel total_qlen_tchan_;	//place to write total_qlen records 
		Tcl_Channel qlen_tchan_;	//place to write per-queue qlen records 
		void trace_total_qlen();	//routine to write total qlen records 
		void trace_qlen();	//routine to write per-queue qlen records 
};

#endif