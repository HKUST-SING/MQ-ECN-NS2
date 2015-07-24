#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <net/netlink.h>
#include <linux/pkt_sched.h>
#include <net/sch_generic.h>
#include <net/pkt_sched.h>
#include <linux/ip.h>
#include <net/dsfield.h>
#include <net/inet_ecn.h>

#include "params.h"

struct dwrr_rate_cfg 
{
	u64 rate_bps;	//bit per second  
	u32 mult;
	u32 shift;
};

struct dwrr_class
{
	int id; //id of this queue
	struct Qdisc	*qdisc;	//inner FIFO queue
	u32	deficitCounter;	//deficit counter of this queue (bytes)
	u8 active;	//whether the queue is not ampty (1) or not (0)
	u8 curr;	//whether this queue is crr being served 
	u32 len_bytes;	//queue length in bytes
	u64 start_time_ns;	//time when this queue is inserted to active list 
	u64 last_pkt_time_ns;	//time when this queue transmits the last packet
	u64 last_pkt_len_ns;	//length of last packet/rate  
	
	struct list_head alist;	//structure of active link list
};

struct dwrr_sched_data 
{
/* Parameters */
	struct dwrr_class *queues; 
	struct dwrr_rate_cfg rate;	
	struct list_head activeList;	//The head point of link list for active queues
	
/* Variables */
	u64 tokens;	//Tokens in nanoseconds  
	u32 sum_len_bytes;	//The sum of lengh of all queues in bytes 
	struct Qdisc *sch; 
	u64	time_ns;	//Time check-point  
	struct qdisc_watchdog watchdog;	//Watchdog timer 
	u64 round_time_ns;	//Estimation of round time
};

/* 
 * We use this function to account for the true number of bytes sent on wire.
 * 20=frame check sequence(8B)+Interpacket gap(12B)
 * 4=Frame check sequence (4B)
 * DWRR_QDISC_MIN_PKT_BYTES=Minimum Ethernet frame size (64B)
 */
static inline unsigned int skb_size(struct sk_buff *skb)
{
	return max_t(unsigned int, skb->len+4,DWRR_QDISC_MIN_PKT_BYTES)+20;
}

/* Borrow from ptb */
static inline void dwrr_qdisc_precompute_ratedata(struct dwrr_rate_cfg *r)
{
	r->shift = 0;
	r->mult = 1;

	if (r->rate_bps > 0) 
	{
		r->shift = 15;
		r->mult = div64_u64(8LLU * NSEC_PER_SEC * (1 << r->shift), r->rate_bps);
	}
}

/* Borrow from ptb: length (bytes) to time (nanosecond) */
static inline u64 l2t_ns(struct dwrr_rate_cfg *r, unsigned int len_bytes)
{
	return ((u64)len_bytes * r->mult) >> r->shift;
}

static inline void dwrr_qdisc_ecn(struct sk_buff *skb)
{
	if(skb_make_writable(skb,sizeof(struct iphdr))&&ip_hdr(skb))
		IP_ECN_set_ce(ip_hdr(skb));
}

static struct dwrr_class* dwrr_qdisc_classify(struct sk_buff *skb, struct Qdisc *sch)
{
	int i=0;
	struct dwrr_sched_data *q = qdisc_priv(sch);	
	struct iphdr* iph=ip_hdr(skb);
	int dscp;
	
	if(unlikely(q->queues==NULL))
		return NULL;
	
	/* Return queue[0] by default*/
	if(unlikely(iph==NULL))
		return &(q->queues[0]);

	dscp=(const int)(iph->tos>>2);
	for(i=0;i<DWRR_QDISC_MAX_QUEUES;i++)
	{
		if(dscp==DWRR_QDISC_QUEUE_DSCP[i])
			return &(q->queues[i]);
	}
	
	return &(q->queues[0]);
}

/* We don't need this */
static struct sk_buff*dwrr_qdisc_peek(struct Qdisc *sch)
{
	return NULL;
}

static struct sk_buff* dwrr_qdisc_dequeue(struct Qdisc *sch)
{
	struct dwrr_sched_data *q = qdisc_priv(sch);	
	struct dwrr_class *cl=NULL; 
	struct sk_buff *skb=NULL;
	u64 sample_ns=0;
	unsigned int len;
	
	/* No active queue */
	if (list_empty(&q->activeList))
		return NULL;
	
	while(1)
	{
		cl=list_first_entry(&q->activeList, struct dwrr_class, alist);
		if(unlikely(cl==NULL))
			return NULL;
		
		/* update deficit counter for this round*/
		if(cl->curr==0)
		{
			cl->curr=1;
			cl->deficitCounter+=DWRR_QDISC_QUEUE_QUANTUM[cl->id];
		}
		
		/* get head packet */
		skb=cl->qdisc->ops->peek(cl->qdisc);
		if(unlikely(skb==NULL)) 
		{
			qdisc_warn_nonwc(__func__, cl->qdisc);
			return NULL;
		}
		
		len=skb_size(skb);		
		/* If this packet can be scheduled by DWRR */
		if(len<=cl->deficitCounter)
		{
			u64 now=ktime_get_ns();
			u64 toks=min_t(u64, now-q->time_ns, DWRR_QDISC_BUCKET_NS)+q->tokens;
			u64 pkt_ns=l2t_ns(&q->rate, len);
			
			/* If we have enough tokens to release this packet */
			if(toks>pkt_ns)
			{
				skb=qdisc_dequeue_peeked(cl->qdisc);
				if (unlikely(skb==NULL))
					return NULL;
				
				q->time_ns=now;
				q->sum_len_bytes-=len;
				cl->len_bytes-=len;
				sch->q.qlen--;
				
				/* Print necessary information in debug mode */
				if(DWRR_QDISC_DEBUG_MODE)
				{
					printk(KERN_INFO "total buffer occupancy %u\n",q->sum_len_bytes);
					printk(KERN_INFO "queue %d buffer occupancy %u\n",cl->id, cl->len_bytes);
				}
				
				/* Bucket */
				q->tokens=min_t(u64,toks-pkt_ns,DWRR_QDISC_BUCKET_NS);
				
				qdisc_unthrottled(sch);
				qdisc_bstats_update(sch, skb);
				
				cl->deficitCounter-=len;
				cl->last_pkt_len_ns=pkt_ns;
				cl->last_pkt_time_ns=ktime_get_ns();
				if(cl->qdisc->q.qlen==0)
				{
					cl->active=0;
					cl->curr=0;
					list_del(&cl->alist);
					sample_ns=max_t(u64, cl->last_pkt_time_ns-cl->start_time_ns,cl->last_pkt_len_ns);
					/* Smooth round time using exponential filter */ 
					q->round_time_ns=(DWRR_QDISC_ROUND_ALPHA*q->round_time_ns+(1000-DWRR_QDISC_ROUND_ALPHA)*sample_ns)/1000;
					/* Print necessary information in debug mode */
					if(DWRR_QDISC_DEBUG_MODE)
					{
						printk(KERN_INFO "sample round time %llu \n",sample_ns);
						printk(KERN_INFO "round time %llu\n",q->round_time_ns);
					}
				}
				
				//printk(KERN_INFO "Dequeue from queue %d\n",cl->id);
				return skb;
			}
			/* if we don't have enough tokens to realse this packet */
			else
			{
				/* we use now+t due to absolute mode of hrtimer (HRTIMER_MODE_ABS) */
				qdisc_watchdog_schedule_ns(&q->watchdog,now+pkt_ns-toks,true);
				qdisc_qstats_overlimit(sch);
				return NULL;
			}
		}
		/* This packet can not be scheduled by DWRR */
		else
		{
			cl->curr=0;
			sample_ns=max_t(u64, cl->last_pkt_time_ns-cl->start_time_ns,cl->last_pkt_len_ns);
			/* Smooth round time using exponential filter */ 
			q->round_time_ns=(DWRR_QDISC_ROUND_ALPHA*q->round_time_ns+(1000-DWRR_QDISC_ROUND_ALPHA)*sample_ns)/1000;
			/* Print necessary information in debug mode */
			if(DWRR_QDISC_DEBUG_MODE)
			{
				printk(KERN_INFO "sample round time %llu\n",sample_ns);
				printk(KERN_INFO "round time %llu\n",q->round_time_ns);
			}
			cl->start_time_ns=ktime_get_ns();
			list_move_tail(&cl->alist, &q->activeList);
		}
	}
	
	return NULL;
}

static int dwrr_qdisc_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct dwrr_class *cl=NULL; 
	unsigned int len=skb_size(skb);
	struct dwrr_sched_data *q = qdisc_priv(sch);	
	int ret;
	u64 ecn_thresh_bytes=0;
	
	cl=dwrr_qdisc_classify(skb,sch);

	/* No appropriate queue or per port shared buffer is overfilled or per queue static buffer is overfilled */
	if(cl==NULL
	||(DWRR_QDISC_BUFFER_MODE==DWRR_QDISC_SHARED_BUFFER&&q->sum_len_bytes+len>DWRR_QDISC_SHARED_BUFFER_BYTES)
	||(DWRR_QDISC_BUFFER_MODE==DWRR_QDISC_STATIC_BUFFER&&cl->len_bytes+len>DWRR_QDISC_QUEUE_BUFFER_BYTES[cl->id]))
	{
		qdisc_qstats_drop(sch);
		qdisc_qstats_drop(cl->qdisc);
		kfree_skb(skb);
		return NET_XMIT_DROP;
	}
	else
	{		
		ret=qdisc_enqueue(skb, cl->qdisc);
		if(ret == NET_XMIT_SUCCESS) 
		{
			/* Per-queue ECN marking */
			if((DWRR_QDISC_ECN_SCHEME==DWRR_QDISC_QUEUE_ECN)&&(cl->len_bytes>DWRR_QDISC_QUEUE_ECN_THRESH[cl->id]*DWRR_QDISC_MTU_BYTES))
			{
				//printk(KERN_INFO "ECN marking\n");
				dwrr_qdisc_ecn(skb);
			}
			/* Per-port ECN marking */
			else if((DWRR_QDISC_ECN_SCHEME==DWRR_QDISC_PORT_ECN)&&(q->sum_len_bytes>DWRR_QDISC_PORT_ECN_THRESH*DWRR_QDISC_MTU_BYTES))
			{
				dwrr_qdisc_ecn(skb);
			}
			/* MQ-ECN */
			else if(DWRR_QDISC_ECN_SCHEME==DWRR_QDISC_MQ_ECN&&q->round_time_ns>0)
			{
				ecn_thresh_bytes=min_t(u64, DWRR_QDISC_QUEUE_QUANTUM[cl->id]*8000000000/q->round_time_ns,q->rate.rate_bps)*DWRR_QDISC_PORT_ECN_THRESH*DWRR_QDISC_MTU_BYTES/q->rate.rate_bps;
				if(cl->len_bytes>ecn_thresh_bytes)
				{
					dwrr_qdisc_ecn(skb);
				}
				
				if(DWRR_QDISC_DEBUG_MODE)
				{
					printk(KERN_INFO "queue %d quantum %d ECN threshold %llu\n", cl->id, DWRR_QDISC_QUEUE_QUANTUM[cl->id],ecn_thresh_bytes); 
				}
			}
			
			/* Update queue sizes */
			sch->q.qlen++;
			q->sum_len_bytes+=len;
			cl->len_bytes+=len;
			
			if(cl->active==0)
			{
				cl->deficitCounter=0;
				cl->active=1;
				cl->curr=0;
				cl->start_time_ns=ktime_get_ns();
				list_add_tail(&(cl->alist),&(q->activeList));
			}
		}
		else
		{
			if (net_xmit_drop_count(ret))
			{
				qdisc_qstats_drop(sch);
				qdisc_qstats_drop(cl->qdisc);
			}
		}
		return ret;
	}
}

/* We don't need this */
static unsigned int dwrr_qdisc_drop(struct Qdisc *sch)
{
	return 0;
}

/* We don't need this */
static int dwrr_qdisc_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	return 0;
}

/* Release Qdisc resources */
static void dwrr_qdisc_destroy(struct Qdisc *sch)
{
	struct dwrr_sched_data *q = qdisc_priv(sch);
	int i;

	if(q->queues!=NULL)
	{
		for(i=0; i<DWRR_QDISC_MAX_QUEUES && (q->queues[i]).qdisc; i++)
			qdisc_destroy((q->queues[i]).qdisc);

		kfree(q->queues);
		q->queues=NULL;
	}
	qdisc_watchdog_cancel(&q->watchdog);
}

static const struct nla_policy dwrr_qdisc_policy[TCA_TBF_MAX + 1] = {
	[TCA_TBF_PARMS] = { .len = sizeof(struct tc_tbf_qopt) },
	[TCA_TBF_RTAB]	= { .type = NLA_BINARY, .len = TC_RTAB_SIZE },
	[TCA_TBF_PTAB]	= { .type = NLA_BINARY, .len = TC_RTAB_SIZE },
};

/* We only leverage TC netlink interface to configure rate */
static int dwrr_qdisc_change(struct Qdisc *sch, struct nlattr *opt)
{
	int err;
	struct dwrr_sched_data *q = qdisc_priv(sch);
	struct nlattr *tb[TCA_TBF_PTAB + 1];
	struct tc_tbf_qopt *qopt;
	__u32 rate;
	
	err = nla_parse_nested(tb, TCA_TBF_PTAB, opt, dwrr_qdisc_policy);
	if(err < 0)
		return err;
	
	err = -EINVAL;
	if (tb[TCA_TBF_PARMS] == NULL)
		goto done;
	
	qopt = nla_data(tb[TCA_TBF_PARMS]);
	rate = qopt->rate.rate;
	/* convert from bytes/s to b/s */
	q->rate.rate_bps=(u64)rate << 3;
	dwrr_qdisc_precompute_ratedata(&q->rate);
	err=0;
	printk(KERN_INFO "sch_dwrr: rate %llu Mbps\n", q->rate.rate_bps/1000000);
	
 done:
	return err;
}

/* Initialize Qdisc */
static int dwrr_qdisc_init(struct Qdisc *sch, struct nlattr *opt)
{
	int i;
	struct dwrr_sched_data *q = qdisc_priv(sch);
	struct Qdisc *child;
	
	if(sch->parent != TC_H_ROOT)
		return -EOPNOTSUPP;
	
	q->queues=kcalloc(DWRR_QDISC_MAX_QUEUES, sizeof(struct dwrr_class),GFP_KERNEL);
	if(q->queues == NULL)
		return -ENOMEM;
	
	q->tokens=0;
	q->time_ns=ktime_get_ns();
	q->sum_len_bytes=0;	//Total buffer occupation
	q->round_time_ns=0;	//Estimation of round time
	q->sch=sch;
	qdisc_watchdog_init(&q->watchdog, sch);
	INIT_LIST_HEAD(&(q->activeList));
	
	for(i=0;i<DWRR_QDISC_MAX_QUEUES;i++)
	{
		/* bfifo is in bytes */
		child=fifo_create_dflt(sch, &bfifo_qdisc_ops, DWRR_QDISC_MAX_BUFFER_BYTES);
		if(child!=NULL)
			(q->queues[i]).qdisc=child;
		else
			goto err;
		
		/* Initialize variables for dwrr_class */
		INIT_LIST_HEAD(&((q->queues[i]).alist));
		(q->queues[i]).id=i;
		(q->queues[i]).deficitCounter=0;
		(q->queues[i]).active=0;
		(q->queues[i]).curr=0;
		(q->queues[i]).len_bytes=0;
		(q->queues[i]).start_time_ns=ktime_get_ns();
		(q->queues[i]).last_pkt_time_ns=ktime_get_ns();
		(q->queues[i]).last_pkt_len_ns=0;
	}
	return dwrr_qdisc_change(sch,opt);
err:
	dwrr_qdisc_destroy(sch);
	return -ENOMEM;
}

static struct Qdisc_ops dwrr_qdisc_ops __read_mostly = {
	.next = NULL,
	.cl_ops = NULL,
	.id = "tbf",
	.priv_size = sizeof(struct dwrr_sched_data),
	.init = dwrr_qdisc_init,
	.destroy = dwrr_qdisc_destroy,
	.enqueue = dwrr_qdisc_enqueue,
	.dequeue = dwrr_qdisc_dequeue,
	.peek=dwrr_qdisc_peek,
	.drop = dwrr_qdisc_drop,
	.change = dwrr_qdisc_change,
	.dump = dwrr_qdisc_dump,
	.owner = THIS_MODULE,
};

static int __init dwrr_qdisc_module_init(void)
{
	if(dwrr_qdisc_params_init()<0)
		return -1;
	printk(KERN_INFO "sch_dwrr: start working\n");
	return register_qdisc(&dwrr_qdisc_ops);
}

static void __exit dwrr_qdisc_module_exit(void)
{
	dwrr_qdisc_params_exit();
	unregister_qdisc(&dwrr_qdisc_ops);
	printk(KERN_INFO "sch_dwrr: stop working\n");
}

module_init(dwrr_qdisc_module_init)
module_exit(dwrr_qdisc_module_exit)
MODULE_LICENSE("GPL");