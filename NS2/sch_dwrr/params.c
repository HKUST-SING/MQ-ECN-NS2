#include "params.h"
#include <linux/sysctl.h>
#include <linux/string.h>

/* Maximum sum of queue lengths. It's similar to total shared buffer per port on switches */
int DWRR_QDISC_MAX_LEN_BYTES=1024*1024;
/* Bucket size in nanosecond */
int DWRR_QDISC_BUCKET_NS=16000; 
/* Per port ECN marking threshold (MTU) */
int DWRR_QDISC_PORT_ECN_THRESH=30;

/* Per queue ECN marking threshold (MTU) */
int DWRR_QDISC_QUEUE_ECN_THRESH[DWRR_QDISC_MAX_QUEUES];
/* DSCP value for different traffic classess */
int DWRR_QDISC_CLASS_DSCP[DWRR_QDISC_MAX_QUEUES];
/* Quantum for different traffic classes*/ 
int DWRR_QDISC_CLASS_QUANTUM[DWRR_QDISC_MAX_QUEUES];

/* All parameters that can be configured through sysctl */
struct DWRR_QDISC_Param DWRR_QDISC_Params[4*DWRR_QDISC_MAX_QUEUES]={
	{"DWRR_QDISC_MAX_LEN_BYTES", &DWRR_QDISC_MAX_LEN_BYTES},
	{"DWRR_QDISC_BUCKET_NS", &DWRR_QDISC_BUCKET_NS},
	{"DWRR_QDISC_PORT_ECN_THRESH", &DWRR_QDISC_PORT_ECN_THRESH},
};

struct ctl_table DWRR_QDISC_Params_table[4*DWRR_QDISC_MAX_QUEUES];
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
		snprintf(DWRR_QDISC_Params[3+i].name, 63,"DWRR_QDISC_QUEUE_ECN_THRESH_%d",i+1);
		//DWRR_QDISC_Params[3+i].name=buffer;
		DWRR_QDISC_Params[3+i].ptr=&DWRR_QDISC_QUEUE_ECN_THRESH[i];
		DWRR_QDISC_QUEUE_ECN_THRESH[i]=30;
		
		/* Initialize DWRR_QDISC_CLASS_DSCP[DWRR_QDISC_MAX_QUEUES] */
		snprintf(DWRR_QDISC_Params[3+i+DWRR_QDISC_MAX_QUEUES].name,63,"DWRR_QDISC_CLASS_DSCP_%d",i+1);
		//DWRR_QDISC_Params[3+i+DWRR_QDISC_MAX_QUEUES].name=buffer;
		DWRR_QDISC_Params[3+i+DWRR_QDISC_MAX_QUEUES].ptr=&DWRR_QDISC_CLASS_DSCP[i];
		DWRR_QDISC_CLASS_DSCP[i]=i;
		
		/* Initialize DWRR_QDISC_CLASS_QUANTUM[DWRR_QDISC_MAX_QUEUES] */
		snprintf(DWRR_QDISC_Params[3+i+2*DWRR_QDISC_MAX_QUEUES].name,63,"DWRR_QDISC_CLASS_QUANTUM_%d",i+1);
		//DWRR_QDISC_Params[3+i+2*DWRR_QDISC_MAX_QUEUES].name=buffer;
		DWRR_QDISC_Params[3+i+2*DWRR_QDISC_MAX_QUEUES].ptr=&DWRR_QDISC_CLASS_QUANTUM[i];
		DWRR_QDISC_CLASS_QUANTUM[i]=(i+1)*DWRR_QDISC_MTU_BYTES;
	}
	
	/* End of the parameters */
	DWRR_QDISC_Params[3+3*DWRR_QDISC_MAX_QUEUES].ptr=NULL;
	
	for(i=0; i<4*DWRR_QDISC_MAX_QUEUES; i++) 
	{
		struct ctl_table *entry = &DWRR_QDISC_Params_table[i];
		
		/* End */
		if(DWRR_QDISC_Params[i].ptr == NULL)
			break;
		
		/* Initialize entry (ctl_table) */
		entry->procname=DWRR_QDISC_Params[i].name;
		entry->data=DWRR_QDISC_Params[i].ptr;
		entry->mode=0644;
		entry->proc_handler=&proc_dointvec;
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
