/*	$NetBSD: uvm_stat.c,v 1.12 1999/03/26 17:34:16 chs Exp $	 */

/*
 *
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Charles D. Cranor and
 *      Washington University.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * from: Id: uvm_stat.c,v 1.1.2.3 1997/12/19 15:01:00 mrg Exp
 */

/*
 * uvm_stat.c
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <vm/vm.h>

#include <uvm/uvm.h>

/*
 * globals
 */

struct uvm_cnt *uvm_cnt_head = NULL;

#ifdef UVMHIST
struct uvm_history_head uvm_histories;
#endif

#ifdef UVMHIST_PRINT
int uvmhist_print_enabled = 1;
#endif

/*
 * prototypes
 */

#ifdef UVMHIST
void uvmhist_dump __P((struct uvm_history *));
void uvm_hist __P((u_int32_t));
static void uvmhist_dump_histories __P((struct uvm_history *[]));
#endif
void uvmcnt_dump __P((void));
void uvm_dump   __P((void));


#ifdef UVMHIST
/* call this from ddb */
void
uvmhist_dump(l)
	struct uvm_history *l;
{
	int lcv, s;

	s = splhigh();
	lcv = l->f;
	do {
		if (l->e[lcv].fmt)
			uvmhist_print(&l->e[lcv]);
		lcv = (lcv + 1) % l->n;
	} while (lcv != l->f);
	splx(s);
}

/*
 * print a merged list of uvm_history structures
 */
static void
uvmhist_dump_histories(hists)
	struct uvm_history *hists[];
{
	struct timeval  tv;
	int	cur[MAXHISTS];
	int	s, lcv, hi;

	/* so we don't get corrupted lists! */
	s = splhigh();

	/* find the first of each list */
	for (lcv = 0; hists[lcv]; lcv++)
		 cur[lcv] = hists[lcv]->f;

	/*
	 * here we loop "forever", finding the next earliest
	 * history entry and printing it.  cur[X] is the current
	 * entry to test for the history in hists[X].  if it is
	 * -1, then this history is finished.
	 */
	for (;;) {
		hi = -1;
		tv.tv_sec = tv.tv_usec = 0;

		/* loop over each history */
		for (lcv = 0; hists[lcv]; lcv++) {
restart:
			if (cur[lcv] == -1)
				continue;

			/*
			 * if the format is empty, go to the next entry
			 * and retry.
			 */
			if (hists[lcv]->e[cur[lcv]].fmt == NULL) {
				cur[lcv] = (cur[lcv] + 1) % (hists[lcv]->n);
				if (cur[lcv] == hists[lcv]->f)
					cur[lcv] = -1;
				goto restart;
			}
				
			/*
			 * if the time hasn't been set yet, or this entry is
			 * earlier than the current tv, set the time and history
			 * index.
			 */
			if (tv.tv_sec == 0 ||
			    timercmp(&hists[lcv]->e[cur[lcv]].tv, &tv, <)) {
				tv = hists[lcv]->e[cur[lcv]].tv;
				hi = lcv;
			}
		}

		/* if we didn't find any entries, we must be done */
		if (hi == -1)
			break;

		/* print and move to the next entry */
		uvmhist_print(&hists[hi]->e[cur[hi]]);
		cur[hi] = (cur[hi] + 1) % (hists[hi]->n);
		if (cur[hi] == hists[hi]->f)
			cur[hi] = -1;
	}
	
	/* done! */
	splx(s);
}

/*
 * call this from ddb.  `bitmask' is from <uvm/uvm_stat.h>.  it
 * merges the named histories.
 */
void
uvm_hist(bitmask)
	u_int32_t	bitmask;	/* XXX only support 32 hists */
{
	struct uvm_history *hists[MAXHISTS + 1];
	int i = 0;

	if ((bitmask & UVMHIST_MAPHIST) || bitmask == 0)
		hists[i++] = &maphist;

	if ((bitmask & UVMHIST_PDHIST) || bitmask == 0)
		hists[i++] = &pdhist;

	hists[i] = NULL;

	uvmhist_dump_histories(hists);
}
#endif /* UVMHIST */

void
uvmcnt_dump()
{
	struct uvm_cnt *uvc = uvm_cnt_head;

	while (uvc) {
		if ((uvc->t & UVMCNT_MASK) != UVMCNT_CNT)
			continue;
		printf("%s = %d\n", uvc->name, uvc->c);
		uvc = uvc->next;
	}
}

/*
 * uvm_dump: ddb hook to dump interesting uvm counters
 */
void 
uvm_dump()
{

	printf("Current UVM status:\n");
	printf("  pagesize=%d (0x%x), pagemask=0x%x, pageshift=%d\n",
	    uvmexp.pagesize, uvmexp.pagesize, uvmexp.pagemask,
	    uvmexp.pageshift);
	printf("  %d VM pages: %d active, %d inactive, %d wired, %d free\n",
	    uvmexp.npages, uvmexp.active, uvmexp.inactive, uvmexp.wired,
	    uvmexp.free);
	printf("  freemin=%d, free-target=%d, inactive-target=%d, "
	    "wired-max=%d\n", uvmexp.freemin, uvmexp.freetarg, uvmexp.inactarg,
	    uvmexp.wiredmax);
	printf("  faults=%d, traps=%d, intrs=%d, ctxswitch=%d\n",
	    uvmexp.faults, uvmexp.traps, uvmexp.intrs, uvmexp.swtch);
	printf("  softint=%d, syscalls=%d, swapins=%d, swapouts=%d\n",
	    uvmexp.softs, uvmexp.syscalls, uvmexp.swapins, uvmexp.swapouts);

	printf("  fault counts:\n");
	printf("    noram=%d, noanon=%d, pgwait=%d, pgrele=%d\n",
	    uvmexp.fltnoram, uvmexp.fltnoanon, uvmexp.fltpgwait,
	    uvmexp.fltpgrele);
	printf("    ok relocks(total)=%d(%d), anget(retrys)=%d(%d), "
	    "amapcopy=%d\n", uvmexp.fltrelckok, uvmexp.fltrelck,
	    uvmexp.fltanget, uvmexp.fltanretry, uvmexp.fltamcopy);
	printf("    neighbor anon/obj pg=%d/%d, gets(lock/unlock)=%d/%d\n",
	    uvmexp.fltnamap, uvmexp.fltnomap, uvmexp.fltlget, uvmexp.fltget);
	printf("    cases: anon=%d, anoncow=%d, obj=%d, prcopy=%d, przero=%d\n",
	    uvmexp.flt_anon, uvmexp.flt_acow, uvmexp.flt_obj, uvmexp.flt_prcopy,
	    uvmexp.flt_przero);

	printf("  daemon and swap counts:\n");
	printf("    woke=%d, revs=%d, scans=%d, swout=%d\n", uvmexp.pdwoke,
	    uvmexp.pdrevs, uvmexp.pdscans, uvmexp.pdswout);
	printf("    busy=%d, freed=%d, reactivate=%d, deactivate=%d\n",
	    uvmexp.pdbusy, uvmexp.pdfreed, uvmexp.pdreact, uvmexp.pddeact);
	printf("    pageouts=%d, pending=%d, nswget=%d\n", uvmexp.pdpageouts,
	    uvmexp.pdpending, uvmexp.nswget);
	printf("    nswapdev=%d, nanon=%d, nfreeanon=%d\n", uvmexp.nswapdev,
	    uvmexp.nanon, uvmexp.nfreeanon);
	printf("    swpages=%d, swpginuse=%d, swpgonly=%d paging=%d\n",
	    uvmexp.swpages, uvmexp.swpginuse, uvmexp.swpgonly, uvmexp.paging);

	printf("  kernel pointers:\n");
	printf("    objs(kern/kmem/mb)=%p/%p/%p\n", uvm.kernel_object,
	    uvmexp.kmem_object, uvmexp.mb_object);
}
