/* $NetBSD: debug.c,v 1.2 1996/03/08 20:14:48 mark Exp $ */

/*
 * Copyright (c) 1994 Melvin Tang-Richardson (Nut)
 * Copyright (c) 1994 Mark Brinicombe
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
 *	This product includes software developed by RiscBSD.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY RISCBSD ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * debug.c
 *
 * Debugging functions
 *
 * Created      : 11/10/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/mount.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

int
debug_count_processes_on_q(queue)
	int queue;
{
	struct proc *lastproc, *queue_head;
	int counter;
	int s;

	counter = 0;
	s = splhigh();
	queue_head=(struct proc *)&qs[queue];
	for (lastproc=qs[queue].ph_link;lastproc!=queue_head;lastproc=lastproc->p_forw) {
		if (lastproc != queue_head)
			printf("Process queue=%8x proc=%08x p_addr=%8x, comm=%s\n",
			    queue, (u_int) lastproc, (u_int) lastproc->p_addr,
			    lastproc->p_comm);
		counter++;
	}
	(void)splx(s);
	return(counter);
}

void
debug_show_q_details()
{
	int counter=0;
	int s;
	
	s = splhigh();
	for (counter=0; counter<32; counter++ )
 		debug_count_processes_on_q(counter);

	if (whichqs == 0)
		printf("queues empty\n");
	(void)splx(s);
}

void
debug_show_all_procs(argc, argv)
	int argc;
	char *argv[];
{
	int np;
	struct proc *ap, *p, *pp;
	int s;
	
	s = splhigh();

	np = nprocs;
	p = ap = (struct proc *)allproc.lh_first;
	if (argc > 1)
		printf("  pid   proc     addr      map      pcb     pmap     comm       wchan\n");
	else
		printf("  pid   proc     addr     uid  ppid  pgrp    flag  stat comm          cputime  \n");
	while (--np >= 0) {
		pp = p->p_pptr;
		if (pp == 0)
			pp = p;
		if (p->p_stat) {
			if (argc > 1)
				printf("%5d %08x %08x %08x %08x %08x %12s  ",
				    p->p_pid, (u_int) ap, (u_int)p->p_addr,
				    (u_int) p->p_vmspace,
				    (u_int) &p->p_addr->u_pcb, (p->p_vmspace ? (u_int)&p->p_vmspace->vm_pmap : 0),
				    ((p->p_comm == 0) ? "..." : p->p_comm));
			else
				printf("%5d %08x %08x %5d %5d %5d  %08x  %d  %12s %5u.%02d  ",
				    p->p_pid, (u_int) ap, (u_int) p->p_addr,
				    p->p_cred->p_ruid,
				    pp->p_pid, p->p_pgrp->pg_id, p->p_flag,
				    p->p_stat, p->p_comm,
				    (u_int)p->p_rtime.tv_sec,
				    (u_int)p->p_rtime.tv_usec / 10000);
			if (p->p_wchan && argc > 1) {
				if (p->p_wmesg)
					printf("%12s ", p->p_wmesg);
				printf("%x", (u_int)p->p_wchan);
			}
			printf("\n");
		}
		ap = p->p_list.le_next;
		if (ap == 0 && np > 0)
			ap = (struct proc*)zombproc.lh_first;
		p = ap;
	}
	(void)splx(s);
}


void
debug_show_callout(argc, argv)
	int argc;
	char *argv[];
{
	register struct callout *p1;
	register int    cum;
	register int    s;
	register int	t;

	s = splhigh();
	printf("      cum     ticks   func     arg\n");
	for (cum = 0, p1 = calltodo.c_next; p1; p1 = p1->c_next) {
		t = p1->c_time;
		if (t > 0)
			cum += t;
		printf("%9d %9d %08x %08x\n", cum, t, (u_int) p1->c_func,
		    (u_int) p1->c_arg);
	}
	(void)splx(s);
}

void
debug_show_fs(argc, argv)
	int argc;
	char *argv[];
{
	struct vfsops **vfsp;
	int s;
	
	s = splhigh();

	printf("Registered filesystems (%d)\n", nvfssw);
         
	for (vfsp = &vfssw[0]; vfsp < &vfssw[nvfssw]; vfsp++) {
		if (*vfsp == NULL)
			continue;
		printf("  %s\n", (*vfsp)->vfs_name);
	}
	(void)splx(s);
}


void
debug_show_vm_map(map, text)
	vm_map_t map;
	char *text;
{
	vm_map_entry_t mapentry;
	int s;
	
	s = splhigh();
    
	printf("vm_map dump : %s\n", text);

	mapentry = &map->header;

	do {
		printf("vm_map_entry: start = %08x end = %08x\n",
		    (u_int) mapentry->start, (u_int) mapentry->end);
		mapentry = mapentry->next;
	} while (mapentry != &map->header);
	(void)splx(s);
}


void
debug_show_pmap(pmap)
	pmap_t pmap;
{
	u_int loop;
	u_int loop1;
	u_int start;
	pt_entry_t *pt;
	pd_entry_t *pd;
	int s;
	
	s = splhigh();

	pd = (pd_entry_t *)pmap;

	printf("pdir=%08x\n", (u_int) pd);    
	for (loop = 0; loop < 4096; ++loop) {
		if (pd[loop] == 0)
			continue;
		printf("%08x : %08x\n", loop * 1024*1024, pd[loop]);
		if ((pd[loop] & 0xff) == 0x11) {
			pt = (pt_entry_t *)(PROCESS_PAGE_TBLS_BASE
			    + loop * 1024);
			loop1 = 0;
			while (loop1 < 256) {
				if (pt[loop1]) {
					start = loop1;
					++loop1;
					while (loop1 < 256 && pt[loop1])
						++loop1;
					printf("  %08x -> %08x\n",
					  loop * 1024*1024 + start * 4096,
					  loop * 1024*1024 + loop1 * 4096 - 1);
				}
				++loop1;
			}
		}
	}
	(void)splx(s);
}

/* End of debug.c */
