#ifndef __PARAMS_H__
#define __PARAMS_H__

#include <linux/types.h>

/* Our module has at most 8 queues */
#define DWRR_QDISC_MAX_QUEUES 8
/* MTU(1500B)+Ethernet header(14B)+Frame check sequence (4B)+Frame check sequence(8B)+Interpacket gap(12B) */
#define DWRR_QDISC_MTU_BYTES 1538
/* Ethernet packets with less than the minimum 64 bytes (header (14B) + user data + FCS (4B)) are padded to 64 bytes. */ 
#define DWRR_QDISC_MIN_PKT_BYTES 64	

/* Maximum sum of queue lengths. It's similar to total shared buffer per port on switches */
extern int DWRR_QDISC_MAX_LEN_BYTES;
/* Bucket size in nanosecond*/
extern int DWRR_QDISC_BUCKET_NS; 
/* Per port ECN marking threshold (MTU) */
extern int DWRR_QDISC_PORT_ECN_THRESH;

/* Per queue ECN marking threshold (MTU) */
extern int DWRR_QDISC_QUEUE_ECN_THRESH[DWRR_QDISC_MAX_QUEUES];
/* DSCP value for different traffic classes */
extern int DWRR_QDISC_CLASS_DSCP[DWRR_QDISC_MAX_QUEUES];
/* Quantum for different traffic classes */ 
extern int DWRR_QDISC_CLASS_QUANTUM[DWRR_QDISC_MAX_QUEUES];

struct DWRR_QDISC_Param
{
	char name[64];
	int *ptr;
};

extern struct DWRR_QDISC_Param DWRR_QDISC_Params[4*DWRR_QDISC_MAX_QUEUES];	

/* Intialize parameters and register sysctl */
int dwrr_qdisc_params_init(void);
/* Unregister sysctl */
void dwrr_qdisc_params_exit(void);

#endif