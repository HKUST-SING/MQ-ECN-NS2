#ifndef __PARAMS_H__
#define __PARAMS_H__

#include <linux/types.h>

/* Our module has at most 8 queues */
#define DWRR_QDISC_MAX_QUEUES 8

/* MTU(1500B)+Ethernet header(14B)+Frame check sequence (4B)+Frame check sequence(8B)+Interpacket gap(12B) */
#define DWRR_QDISC_MTU_BYTES 1538

/* Ethernet packets with less than the minimum 64 bytes (header (14B) + user data + FCS (4B)) are padded to 64 bytes. */ 
#define DWRR_QDISC_MIN_PKT_BYTES 64	

/* Per-port ECN marking threshold in bytes */
int DWRR_QDISC_ECN_THRESH_BYTES=20*DWRR_QDISC_MTU_BYTES;
/* Maximum sum of queue lenghs. It's similar to shared buffer per port on switches */
int DWRR_QDISC_MAX_LEN_BYTES=1024*1024;
/* Bucket size in nanosecond */
int DWRR_QDISC_BUCKET_NS=2000000; ; 

/* DSCP value for different queues */
int DWRR_QDISC_PRIO_DSCP_1=0;
int DWRR_QDISC_PRIO_DSCP_2=1;
int DWRR_QDISC_PRIO_DSCP_3=2;
int DWRR_QDISC_PRIO_DSCP_4=3;
int DWRR_QDISC_PRIO_DSCP_5=4;
int DWRR_QDISC_PRIO_DSCP_6=5;
int DWRR_QDISC_PRIO_DSCP_7=6;
int DWRR_QDISC_PRIO_DSCP_8=7;

#endif