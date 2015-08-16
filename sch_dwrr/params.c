#include "params.h"
#include <linux/sysctl.h>
#include <linux/string.h>

/* Debug mode or not. By default, we disable debug mode */
int DWRR_QDISC_DEBUG_MODE=DWRR_QDISC_DEBUG_OFF;
/* Buffer management mode: shared (0) or static (1). By default, we enable shread buffer. */
int DWRR_QDISC_BUFFER_MODE=DWRR_QDISC_SHARED_BUFFER;
/* Per port shared buffer (bytes) */
int DWRR_QDISC_SHARED_BUFFER_BYTES=DWRR_QDISC_MAX_BUFFER_BYTES;
/* Bucket size in nanosecond. By default, we use 20us for 1G network. */
int DWRR_QDISC_BUCKET_NS=20000;
/* Per port ECN marking threshold (bytes). By default, we use 30KB for 1G network. */
int DWRR_QDISC_PORT_THRESH_BYTES=30000;
/* ECN marking scheme. By default, we use per queue ECN. */
int DWRR_QDISC_ECN_SCHEME=DWRR_QDISC_QUEUE_ECN;
/* Alpha for quantum sum estimation. It is 0.75 by default. */
int DWRR_QDISC_QUANTUM_ALPHA=750;
/* Alpha for round time estimation. It is 0.75 by default. */
int DWRR_QDISC_ROUND_ALPHA=750;

int DWRR_QDISC_DEBUG_MODE_MIN=DWRR_QDISC_DEBUG_OFF;
int DWRR_QDISC_DEBUG_MODE_MAX=DWRR_QDISC_DEBUG_ON;
int DWRR_QDISC_BUFFER_MODE_MIN=DWRR_QDISC_SHARED_BUFFER;
int DWRR_QDISC_BUFFER_MODE_MAX=DWRR_QDISC_STATIC_BUFFER;
int DWRR_QDISC_ECN_SCHEME_MIN=DWRR_QDISC_DISABLE_ECN;
int DWRR_QDISC_ECN_SCHEME_MAX=DWRR_QDISC_MQ_ECN_RR;
int DWRR_QDISC_QUANTUM_ALPHA_MIN=0;
int DWRR_QDISC_QUANTUM_ALPHA_MAX=1000;
int DWRR_QDISC_ROUND_ALPHA_MIN=0;
int DWRR_QDISC_ROUND_ALPHA_MAX=1000;
int DWRR_QDISC_DSCP_MIN=0;
int DWRR_QDISC_DSCP_MAX=63;
int DWRR_QDISC_QUANTUM_MIN=DWRR_QDISC_MTU_BYTES;
int DWRR_QDISC_QUANTUM_MAX=200*1024;

/* Per queue ECN marking threshold (bytes) */
int DWRR_QDISC_QUEUE_THRESH_BYTES[DWRR_QDISC_MAX_QUEUES];
/* DSCP value for different queues*/
int DWRR_QDISC_QUEUE_DSCP[DWRR_QDISC_MAX_QUEUES];
/* Quantum for different queues*/
int DWRR_QDISC_QUEUE_QUANTUM[DWRR_QDISC_MAX_QUEUES];
/* Per queue minimum guarantee buffer (bytes) */
int DWRR_QDISC_QUEUE_BUFFER_BYTES[DWRR_QDISC_MAX_QUEUES];

/* All parameters that can be configured through sysctl. We have 8+4*DWRR_QDISC_MAX_QUEUES parameters in total. */
struct DWRR_QDISC_Param DWRR_QDISC_Params[8+4*DWRR_QDISC_MAX_QUEUES+1]={
	{"debug_mode", &DWRR_QDISC_DEBUG_MODE},
	{"buffer_mode",&DWRR_QDISC_BUFFER_MODE},
	{"shared_buffer_bytes", &DWRR_QDISC_SHARED_BUFFER_BYTES},
	{"bucket_ns", &DWRR_QDISC_BUCKET_NS},
	{"port_thresh_bytes", &DWRR_QDISC_PORT_THRESH_BYTES},
	{"ecn_scheme",&DWRR_QDISC_ECN_SCHEME},
	{"quantum_alpha",&DWRR_QDISC_QUANTUM_ALPHA},
	{"round_alpha",&DWRR_QDISC_ROUND_ALPHA},
};

struct ctl_table DWRR_QDISC_Params_table[8+4*DWRR_QDISC_MAX_QUEUES+1];
struct ctl_path DWRR_QDISC_Params_path[] = {
	{ .procname = "dwrr" },
	{ },
};

struct ctl_table_header *DWRR_QDISC_Sysctl=NULL;

int dwrr_qdisc_params_init()
{
	int i=0;
	memset(DWRR_QDISC_Params_table, 0, sizeof(DWRR_QDISC_Params_table));

	for(i=0;i<DWRR_QDISC_MAX_QUEUES;i++)
	{
		/* Initialize DWRR_QDISC_QUEUE_THRESH_BYTES[DWRR_QDISC_MAX_QUEUES]*/
		snprintf(DWRR_QDISC_Params[8+i].name, 63,"queue_thresh_bytes_%d",i);
		DWRR_QDISC_Params[8+i].ptr=&DWRR_QDISC_QUEUE_THRESH_BYTES[i];
		DWRR_QDISC_QUEUE_THRESH_BYTES[i]=DWRR_QDISC_PORT_THRESH_BYTES;

		/* Initialize DWRR_QDISC_QUEUE_DSCP[DWRR_QDISC_MAX_QUEUES] */
		snprintf(DWRR_QDISC_Params[8+i+DWRR_QDISC_MAX_QUEUES].name,63,"queue_dscp_%d",i);
		DWRR_QDISC_Params[8+i+DWRR_QDISC_MAX_QUEUES].ptr=&DWRR_QDISC_QUEUE_DSCP[i];
		DWRR_QDISC_QUEUE_DSCP[i]=i;

		/* Initialize DWRR_QDISC_QUEUE_QUANTUM[DWRR_QDISC_MAX_QUEUES] */
		snprintf(DWRR_QDISC_Params[8+i+2*DWRR_QDISC_MAX_QUEUES].name,63,"queue_quantum_%d",i);
		DWRR_QDISC_Params[8+i+2*DWRR_QDISC_MAX_QUEUES].ptr=&DWRR_QDISC_QUEUE_QUANTUM[i];
		DWRR_QDISC_QUEUE_QUANTUM[i]=DWRR_QDISC_MTU_BYTES;

		/* Initialize DWRR_QDISC_QUEUE_BUFFER_BYTES[DWRR_QDISC_MAX_QUEUES] */
		snprintf(DWRR_QDISC_Params[8+i+3*DWRR_QDISC_MAX_QUEUES].name,63,"queue_buffer_bytes_%d",i);
		DWRR_QDISC_Params[8+i+3*DWRR_QDISC_MAX_QUEUES].ptr=&DWRR_QDISC_QUEUE_BUFFER_BYTES[i];
		DWRR_QDISC_QUEUE_BUFFER_BYTES[i]=DWRR_QDISC_MAX_BUFFER_BYTES;
	}

	/* End of the parameters */
	DWRR_QDISC_Params[8+4*DWRR_QDISC_MAX_QUEUES].ptr=NULL;

	for(i=0; i<8+4*DWRR_QDISC_MAX_QUEUES+1; i++)
	{
		struct ctl_table *entry = &DWRR_QDISC_Params_table[i];

		/* End */
		if(DWRR_QDISC_Params[i].ptr == NULL)
			break;

		/* Initialize entry (ctl_table) */
		entry->procname=DWRR_QDISC_Params[i].name;
		entry->data=DWRR_QDISC_Params[i].ptr;
		entry->mode=0644;

		/* DWRR_QDISC_DEBUG_MODE */
		if(i==0)
		{
			entry->proc_handler=&proc_dointvec_minmax;
			entry->extra1=&DWRR_QDISC_DEBUG_MODE_MIN;
			entry->extra2=&DWRR_QDISC_DEBUG_MODE_MAX;
		}
		/* DWRR_QDISC_BUFFER_MODE */
		else if(i==1)
		{
			entry->proc_handler=&proc_dointvec_minmax;
			entry->extra1=&DWRR_QDISC_BUFFER_MODE_MIN;
			entry->extra2=&DWRR_QDISC_BUFFER_MODE_MAX;
		}
		/* DWRR_QDISC_ECN_SCHEME */
		else if(i==5)
		{
			entry->proc_handler=&proc_dointvec_minmax;
			entry->extra1=&DWRR_QDISC_ECN_SCHEME_MIN;
			entry->extra2=&DWRR_QDISC_ECN_SCHEME_MAX;
		}
		/* DWRR_QDISC_QUANTUM_ALPHA*/
		else if(i==6)
		{
			entry->proc_handler=&proc_dointvec_minmax;
			entry->extra1=&DWRR_QDISC_QUANTUM_ALPHA_MIN;
			entry->extra2=&DWRR_QDISC_QUANTUM_ALPHA_MAX;
		}
		/* DWRR_QDISC_ROUND_ALPHA */
		else if(i==7)
		{
			entry->proc_handler=&proc_dointvec_minmax;
			entry->extra1=&DWRR_QDISC_ROUND_ALPHA_MIN;
			entry->extra2=&DWRR_QDISC_ROUND_ALPHA_MAX;
		}
		/* DWRR_QDISC_QUEUE_DSCP[] */
		else if(i>=8+DWRR_QDISC_MAX_QUEUES&&i<8+2*DWRR_QDISC_MAX_QUEUES)
		{
			entry->proc_handler=&proc_dointvec_minmax;
			entry->extra1=&DWRR_QDISC_DSCP_MIN;
			entry->extra2=&DWRR_QDISC_DSCP_MAX;
		}
		/* DWRR_QDISC_QUEUE_QUANTUM[] */
		else if(i>=8+2*DWRR_QDISC_MAX_QUEUES&&i<8+3*DWRR_QDISC_MAX_QUEUES)
		{
			entry->proc_handler=&proc_dointvec_minmax;
			entry->extra1=&DWRR_QDISC_QUANTUM_MIN;
			entry->extra2=&DWRR_QDISC_QUANTUM_MAX;
		}
		/*DWRR_QDISC_QUEUE_ECN_THRESH[] and DWRR_QDISC_QUEUE_BUFFER_BYTES[] */
		else
		{
			entry->proc_handler=&proc_dointvec;
		}
		entry->maxlen=sizeof(int);
	}

	DWRR_QDISC_Sysctl=register_sysctl_paths(DWRR_QDISC_Params_path, DWRR_QDISC_Params_table);
	if(unlikely(DWRR_QDISC_Sysctl==NULL))
		return -1;
	else
		return 0;

}

void dwrr_qdisc_params_exit()
{
	if(likely(DWRR_QDISC_Sysctl!=NULL))
		unregister_sysctl_table(DWRR_QDISC_Sysctl);
}
