#ifndef ns_wrr_h
#define ns_wrr_h

#include "queue.h"
#include "config.h"
#include "trace.h"

/*Maximum queue number */
#define MAX_QUEUE_NUM 32

/* Per-queue ECN marking */
#define PER_QUEUE_MARKING 0
/* Per-port ECN marking */
#define PER_PORT_MARKING 1
/* Dynamic per-queue ECN marking scheme */
#define QUEUE_SMART_MARKING 2
/* Dynamic hybrid (per-queue+per-port) ECN marking scheme */
#define HYBRID_SMRT_MARKING 3

class PacketWRR;
class WRR;

class PacketWRR: public PacketQueue
{
	public: 
		PacketWRR(): weight(1),counter(0),thresh(0),active(false),current(false),start_time(0),next(NULL) {} 
		
		int weight;	//weight of this queue 
		int counter;	//counter for packets that can be sent in this round 
		int avgPktSize;	//Average packet size (bytes)
		double thresh;	// per-queue ECN marking threshold (pkts)
		bool active;	//whether this queue is active (qlen>0)
		bool current;	//whether this queue is currently being served (deficitCounter has been updated for thie round)
		double start_time;	//time when this queue is inserted to active list 
		PacketWRR *next;	//pointer to next node
		
		friend class WRR;
};

class WRR : public Queue 
{
	public:
		WRR();
		~WRR();
		virtual int command(int argc, const char*const* argv);
		
	protected:
		Packet *deque(void);
		void enque(Packet *pkt);
		int TotalByteLength();	//Get total length of all queues in bytes 
		int TotalWeight();	//Get sum of weight
		int MarkingECN(int q); //Determine whether we need to mark ECN, q is current queue number 
		
		/* Variables */
		PacketWRR *queues;	//underlying multi-FIFO (CoS) queues
		PacketWRR *activeList;	//list for active queues	
		double round_time;	//Round time estimation value
		bool init;
		
		int queue_num_;	//number of queues 
		int mean_pktsize_;	//MTU in bytes 
		double port_thresh_;	//per-port ECN marking threshold (pkts)
		int marking_scheme_;	//ECN marking policy 
		double estimate_pktsize_alpha_;	//factor between 0 and 1 for average packet size estimation
		double estimate_round_alpha_;	//factor between 0 and 1 for round time estimation
		int estimate_round_filter_;	//filter some round time samples
		double link_capacity_;	//Link capacity
		
		Tcl_Channel total_qlen_tchan_;	//place to write total_qlen records 
		Tcl_Channel qlen_tchan_;	//place to write per-queue qlen records 
		void trace_total_qlen();	//routine to write total qlen records 
		void trace_qlen();	//routine to write per-queue qlen records 
};

#endif