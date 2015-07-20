#include "params.h"
#include <linux/sysctl.h>
#include <linux/string.h>

/* Debug mode or not */
int DWRR_QDISC_DEBUG_MODE=0;
/* Per port shared buffer (bytes) */
int DWRR_QDISC_SHARED_BUFFER_BYTES=150*1024;
/* Bucket size in nanosecond */
int DWRR_QDISC_BUCKET_NS=18000; 
/* Per port ECN marking threshold (MTU) */
int DWRR_QDISC_PORT_ECN_THRESH=30;
/* ECN marking scheme */
int DWRR_QDISC_ECN_SCHEME=DWRR_QDISC_QUEUE_ECN;
/* Alpha for round time estimation */
int DWRR_QDISC_ROUND_ALPHA=750;

int DWRR_QDISC_DEBUG_MODE_MIN=0;
int DWRR_QDISC_DEBUG_MODE_MAX=1;
int DWRR_QDISC_ECN_SCHEME_MIN=DWRR_QDISC_DISABLE_ECN;
int DWRR_QDISC_ECN_SCHEME_MAX=DWRR_QDISC_MQ_ECN;
int DWRR_QDISC_ROUND_ALPHA_MIN=0;
int DWRR_QDISC_ROUND_ALPHA_MAX=1000;
int DWRR_QDISC_DSCP_MIN=0;
int DWRR_QDISC_DSCP_MAX=63;
int DWRR_QDISC_QUANTUM_MIN=DWRR_QDISC_MTU_BYTES;
int DWRR_QDISC_QUANTUM_MAX=100*1024; 

/* Per queue ECN marking threshold (MTU) */
int DWRR_QDISC_QUEUE_ECN_THRESH[DWRR_QDISC_MAX_QUEUES];
/* DSCP value for different traffic classess */
int DWRR_QDISC_CLASS_DSCP[DWRR_QDISC_MAX_QUEUES];
/* Quantum for different traffic classes*/ 
int DWRR_QDISC_CLASS_QUANTUM[DWRR_QDISC_MAX_QUEUES];

/* All parameters that can be configured through sysctl. We have 6+3*DWRR_QDISC_MAX_QUEUES parameters in total. */
struct DWRR_QDISC_Param DWRR_QDISC_Params[6+3*DWRR_QDISC_MAX_QUEUES+1]={
	{"DWRR_QDISC_DEBUG_MODE", &DWRR_QDISC_DEBUG_MODE},
	{"DWRR_QDISC_SHARED_BUFFER_BYTES", &DWRR_QDISC_SHARED_BUFFER_BYTES},
	{"DWRR_QDISC_BUCKET_NS", &DWRR_QDISC_BUCKET_NS},
	{"DWRR_QDISC_PORT_ECN_THRESH", &DWRR_QDISC_PORT_ECN_THRESH},
	{"DWRR_QDISC_ECN_SCHEME",&DWRR_QDISC_ECN_SCHEME},
	{"DWRR_QDISC_ROUND_ALPHA",&DWRR_QDISC_ROUND_ALPHA},
};

struct ctl_table DWRR_QDISC_Params_table[6+3*DWRR_QDISC_MAX_QUEUES+1];
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
		/* Initialize DWRR_QDISC_QUEUE_ECN_THRESH[DWRR_QDISC_MAX_QUEUES]*/
		snprintf(DWRR_QDISC_Params[6+i].name, 63,"DWRR_QDISC_QUEUE_ECN_THRESH_%d",i+1);
		DWRR_QDISC_Params[6+i].ptr=&DWRR_QDISC_QUEUE_ECN_THRESH[i];
		DWRR_QDISC_QUEUE_ECN_THRESH[i]=DWRR_QDISC_PORT_ECN_THRESH;
		
		/* Initialize DWRR_QDISC_CLASS_DSCP[DWRR_QDISC_MAX_QUEUES] */
		snprintf(DWRR_QDISC_Params[6+i+DWRR_QDISC_MAX_QUEUES].name,63,"DWRR_QDISC_CLASS_DSCP_%d",i+1);
		DWRR_QDISC_Params[6+i+DWRR_QDISC_MAX_QUEUES].ptr=&DWRR_QDISC_CLASS_DSCP[i];
		DWRR_QDISC_CLASS_DSCP[i]=i;
		
		/* Initialize DWRR_QDISC_CLASS_QUANTUM[DWRR_QDISC_MAX_QUEUES] */
		snprintf(DWRR_QDISC_Params[6+i+2*DWRR_QDISC_MAX_QUEUES].name,63,"DWRR_QDISC_CLASS_QUANTUM_%d",i+1);
		DWRR_QDISC_Params[6+i+2*DWRR_QDISC_MAX_QUEUES].ptr=&DWRR_QDISC_CLASS_QUANTUM[i];
		DWRR_QDISC_CLASS_QUANTUM[i]=DWRR_QDISC_MTU_BYTES;
	}
	
	/* End of the parameters */
	DWRR_QDISC_Params[6+3*DWRR_QDISC_MAX_QUEUES].ptr=NULL;
	
	for(i=0; i<6+3*DWRR_QDISC_MAX_QUEUES+1; i++) 
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
		/* DWRR_QDISC_ECN_SCHEME */
		else if(i==4)
		{
			entry->proc_handler=&proc_dointvec_minmax;
			entry->extra1=&DWRR_QDISC_ECN_SCHEME_MIN;
			entry->extra2=&DWRR_QDISC_ECN_SCHEME_MAX;
		}
		/* DWRR_QDISC_ROUND_ALPHA */
		else if(i==5)
		{
			entry->proc_handler=&proc_dointvec_minmax;
			entry->extra1=&DWRR_QDISC_ROUND_ALPHA_MIN;
			entry->extra2=&DWRR_QDISC_ROUND_ALPHA_MIN;
		}
		/* DWRR_QDISC_CLASS_DSCP[] */
		else if(i>=6+DWRR_QDISC_MAX_QUEUES&&i<6+2*DWRR_QDISC_MAX_QUEUES)
		{
			entry->proc_handler=&proc_dointvec_minmax;
			entry->extra1=&DWRR_QDISC_DSCP_MIN;
			entry->extra2=&DWRR_QDISC_DSCP_MAX;
		}
		/* DWRR_QDISC_CLASS_QUANTUM[] */
		else if(i>=6+2*DWRR_QDISC_MAX_QUEUES)
		{
			entry->proc_handler=&proc_dointvec_minmax;
			entry->extra1=&DWRR_QDISC_QUANTUM_MIN;
			entry->extra2=&DWRR_QDISC_QUANTUM_MAX;
		}
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
