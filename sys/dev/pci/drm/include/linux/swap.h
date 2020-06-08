/* Public domain. */

#ifndef _LINUX_SWAP_H
#define _LINUX_SWAP_H

/*
 * normally clock.h would be indirectly included via
 *
 * linux/swap.h
 * linux/memcontrol.h
 * linux/writeback.h
 * linux/blk-cgroup.h
 * linux/blkdev.h
 * linux/sched/clock.h
 */
#include <linux/sched/clock.h>

#include <uvm/uvm_extern.h>

static inline long
get_nr_swap_pages(void)
{ 
	return uvmexp.swpages - uvmexp.swpginuse;
}

/* 
 * XXX For now, we don't want the shrinker to be too aggressive, so
 * pretend we're not called from the pagedaemon even if we are.
 */
#define current_is_kswapd()	0

#endif
