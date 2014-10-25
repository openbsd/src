/*	$OpenBSD: uvm_stat.c,v 1.28 2014/10/25 12:54:16 miod Exp $	 */
/*	$NetBSD: uvm_stat.c,v 1.18 2001/03/09 01:02:13 chs Exp $	 */

/*
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

#include <uvm/uvm.h>
#include <uvm/uvm_ddb.h>

#ifdef DDB

/*
 * uvmexp_print: ddb hook to print interesting uvm counters
 */
void
uvmexp_print(int (*pr)(const char *, ...))
{

	(*pr)("Current UVM status:\n");
	(*pr)("  pagesize=%d (0x%x), pagemask=0x%x, pageshift=%d\n",
	    uvmexp.pagesize, uvmexp.pagesize, uvmexp.pagemask,
	    uvmexp.pageshift);
	(*pr)("  %d VM pages: %d active, %d inactive, %d wired, %d free (%d zero)\n",
	    uvmexp.npages, uvmexp.active, uvmexp.inactive, uvmexp.wired,
	    uvmexp.free, uvmexp.zeropages);
	(*pr)("  min  %d%% (%d) anon, %d%% (%d) vnode, %d%% (%d) vtext\n",
	    uvmexp.anonminpct, uvmexp.anonmin, uvmexp.vnodeminpct,
	    uvmexp.vnodemin, uvmexp.vtextminpct, uvmexp.vtextmin);
	(*pr)("  pages  %d anon, %d vnode, %d vtext\n",
	    uvmexp.anonpages, uvmexp.vnodepages, uvmexp.vtextpages);
	(*pr)("  freemin=%d, free-target=%d, inactive-target=%d, "
	    "wired-max=%d\n", uvmexp.freemin, uvmexp.freetarg, uvmexp.inactarg,
	    uvmexp.wiredmax);
	(*pr)("  faults=%d, traps=%d, intrs=%d, ctxswitch=%d fpuswitch=%d\n",
	    uvmexp.faults, uvmexp.traps, uvmexp.intrs, uvmexp.swtch,
	    uvmexp.fpswtch);
	(*pr)("  softint=%d, syscalls=%d, kmapent=%d\n",
	    uvmexp.softs, uvmexp.syscalls, uvmexp.kmapent);

	(*pr)("  fault counts:\n");
	(*pr)("    noram=%d, noanon=%d, pgwait=%d, pgrele=%d\n",
	    uvmexp.fltnoram, uvmexp.fltnoanon, uvmexp.fltpgwait,
	    uvmexp.fltpgrele);
	(*pr)("    ok relocks(total)=%d(%d), anget(retries)=%d(%d), "
	    "amapcopy=%d\n", uvmexp.fltrelckok, uvmexp.fltrelck,
	    uvmexp.fltanget, uvmexp.fltanretry, uvmexp.fltamcopy);
	(*pr)("    neighbor anon/obj pg=%d/%d, gets(lock/unlock)=%d/%d\n",
	    uvmexp.fltnamap, uvmexp.fltnomap, uvmexp.fltlget, uvmexp.fltget);
	(*pr)("    cases: anon=%d, anoncow=%d, obj=%d, prcopy=%d, przero=%d\n",
	    uvmexp.flt_anon, uvmexp.flt_acow, uvmexp.flt_obj, uvmexp.flt_prcopy,
	    uvmexp.flt_przero);

	(*pr)("  daemon and swap counts:\n");
	(*pr)("    woke=%d, revs=%d, scans=%d, obscans=%d, anscans=%d\n",
	    uvmexp.pdwoke, uvmexp.pdrevs, uvmexp.pdscans, uvmexp.pdobscan,
	    uvmexp.pdanscan);
	(*pr)("    busy=%d, freed=%d, reactivate=%d, deactivate=%d\n",
	    uvmexp.pdbusy, uvmexp.pdfreed, uvmexp.pdreact, uvmexp.pddeact);
	(*pr)("    pageouts=%d, pending=%d, nswget=%d\n", uvmexp.pdpageouts,
	    uvmexp.pdpending, uvmexp.nswget);
	(*pr)("    nswapdev=%d, nanon=%d, nanonneeded=%d nfreeanon=%d\n",
	    uvmexp.nswapdev, uvmexp.nanon, uvmexp.nanonneeded,
	    uvmexp.nfreeanon);
	(*pr)("    swpages=%d, swpginuse=%d, swpgonly=%d paging=%d\n",
	    uvmexp.swpages, uvmexp.swpginuse, uvmexp.swpgonly, uvmexp.paging);

	(*pr)("  kernel pointers:\n");
	(*pr)("    objs(kern)=%p\n", uvm.kernel_object);
}
#endif
