#ifndef ns_dwrr_h
#define ns_dwrr_h

#include "queue.h"
#include "config.h"
#include "trace.h"
#include "timer-handler.h"

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

class PacketDWRR;
class DWRR;

class DWRR_Timer: public TimerHandler
{
	public:
		DWRR_Timer(DWRR *q) : TimerHandler() { queue=q;}
	
	protected:
		virtual void expire(Event *e);
		DWRR *queue;
};

class PacketDWRR: public PacketQueue
{
	public: 
		PacketDWRR(): quantum(1500),deficitCounter(0),thresh(0),active(false),backlogged(false),current(false),next(NULL),input_rate(0), input_bytes(0), start_time(0) {} 
		
		int quantum;	//quantum (weight) of this queue 
		int deficitCounter;	//deficit counter for this queue 
		double thresh;	// per-queue ECN marking threshold (pkts)
		bool active;	//whether this queue is active (qlen>0)
		bool backlogged;	//whether this queue is backlogged
		bool current;	//whether this queue is currently being served (deficitCounter has been updated for thie round)
		PacketDWRR *next;	//pointer to next node
		
		double input_rate;	//input rate estimation
		int input_bytes;	//input traffic (bytes) 
		double start_time;	//time when this queue is inserted to active list 
		
		friend class DWRR;
};

class DWRR : public Queue 
{
	public:
		DWRR();
		~DWRR();
		virtual int command(int argc, const char*const* argv);
		void timeout(int);	//expire call back function for timer
		
	protected:
		Packet *deque(void);
		void enque(Packet *pkt);
		int TotalByteLength();	//Get total length of all queues in bytes 
		int MarkingECN(int q); //Determine whether we need to mark ECN, q is current queue number 
		
		/* Variables */
		PacketDWRR *queues;	//underlying multi-FIFO (CoS) queues
		PacketDWRR *activeList;	//list for active queues	
		int init;
		
		int queue_num_;	//number of queues 
		int mean_pktsize_;	//MTU in bytes 
		double port_thresh_;	//per-port ECN marking threshold (pkts)
		int marking_scheme_;	//ECN marking policy 
		int round_;	//Number of rounds for backlogged judgement
		double round_time_;	//round time estimation value
		double estimate_rate_period_;	//period to estimate rate
		double estimate_rate_alpha_;	//factor between 0 and 1 for rate estimation
		double estimate_round_alpha_;	//factor between 0 and 1 for round time estimation
		
		DWRR_Timer timer;	//timer to estimate rate
		
		Tcl_Channel total_qlen_tchan_;	//place to write total_qlen records 
		Tcl_Channel qlen_tchan_;	//place to write per-queue qlen records 
		void trace_total_qlen();	//routine to write total qlen records 
		void trace_qlen();	//routine to write per-queue qlen records 
};

#endif