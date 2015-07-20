#ifndef __PARAMS_H__
#define __PARAMS_H__

#include <linux/types.h>

/* Our module has at most 8 queues */
#define DWRR_QDISC_MAX_QUEUES 8
/* MTU(1500B)+Ethernet header(14B)+Frame check sequence (4B)+Frame check sequence(8B)+Interpacket gap(12B) */
#define DWRR_QDISC_MTU_BYTES 1538
/* Ethernet packets with less than the minimum 64 bytes (header (14B) + user data + FCS (4B)) are padded to 64 bytes. */ 
#define DWRR_QDISC_MIN_PKT_BYTES 64	
/* Maximum buffer size (2MB)*/
#define DWRR_QDISC_MAX_BUFFER_BYTES 2000000

/* Disable ECN marking */
#define DWRR_QDISC_DISABLE_ECN 0
/* Per queue ECN marking */
#define DWRR_QDISC_QUEUE_ECN 1
/* Per port ECN marking */
#define DWRR_QDISC_PORT_ECN 2
/* Our scheme: MQ-ECN */
#define DWRR_QDISC_MQ_ECN 3

/* Debug mode or not */
extern int DWRR_QDISC_DEBUG_MODE;
/* Per port shared buffer (bytes) */
extern int DWRR_QDISC_SHARED_BUFFER_BYTES;
/* Bucket size in nanosecond*/
extern int DWRR_QDISC_BUCKET_NS; 
/* Per port ECN marking threshold (MTU) */
extern int DWRR_QDISC_PORT_ECN_THRESH;
/* ECN marking scheme */
extern int DWRR_QDISC_ECN_SCHEME;
/* Alpha for round time estimation */
extern int DWRR_QDISC_ROUND_ALPHA;

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

extern struct DWRR_QDISC_Param DWRR_QDISC_Params[6+3*DWRR_QDISC_MAX_QUEUES+1];	

/* Intialize parameters and register sysctl */
int dwrr_qdisc_params_init(void);
/* Unregister sysctl */
void dwrr_qdisc_params_exit(void);

#endif