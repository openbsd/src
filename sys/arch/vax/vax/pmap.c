/*      $NetBSD: pmap.c,v 1.17 1995/08/22 04:34:17 ragge Exp $     */
#define DEBUG
/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */

 /* All bugs are subject to removal without further notice */
		
#include "sys/types.h"
#include "sys/param.h"
#include "sys/queue.h"
#include "sys/malloc.h"
#include "sys/proc.h"
#include "sys/user.h"
#include "sys/msgbuf.h"
#include "vm/vm.h"
#include "vm/vm_page.h"
#include "vm/vm_kern.h"
#include "vax/include/pte.h"
#include "vax/include/pcb.h"
#include "vax/include/mtpr.h"
#include "vax/include/loconf.h"
#include "vax/include/macros.h"
#include "vax/include/sid.h"

#include "uba.h"

pt_entry_t *pmap_virt2pte(pmap_t, u_int);

#define	PTE_TO_PV(pte)	(PHYS_TO_PV((pte&PG_FRAME)<<PG_SHIFT))


struct pmap kernel_pmap_store;
unsigned int gurkskit[50],istack;

static pv_entry_t alloc_pv_entry();
static void	free_pv_entry();

static int prot_array[]={ PG_NONE, PG_RO,   PG_RW,   PG_RW,
			  PG_RO,   PG_RO,   PG_RW,   PG_RW };
    
static int kernel_prot[]={ PG_NONE, PG_KR, PG_KW, PG_KW,
				PG_RO,PG_KR,PG_KW,PG_URKW};

static pv_entry_t   pv_head =NULL;
static unsigned int pv_count=0;
vm_offset_t ptemapstart,ptemapend;

extern uint etext;
extern u_int *pte_cmap;
extern int  maxproc;
extern struct vmspace vmspace0;
extern int edata, end;
uint*  UMEMmap;
void*  Numem;
void  *scratch;
uint   sigsida;
#ifdef DEBUG
int startpmapdebug=0;
extern int startsysc, faultdebug;
#endif
unsigned int *valueptr=gurkskit, vmmap;
pt_entry_t *Sysmap;
vm_map_t	pte_map;

vm_offset_t     avail_start, avail_end;
vm_offset_t   virtual_avail, virtual_end; /* Available virtual memory   */

/*
 * pmap_bootstrap().
 * Called as part of vm bootstrap, allocates internal pmap structures.
 * Assumes that nothing is mapped, and that kernel stack is located
 * immediately after end.
 */

void 
pmap_bootstrap()
{
	uint	i;
	extern	u_int sigcode, esigcode, proc0paddr;
	extern char *esym;
	struct pmap *p0pmap=&vmspace0.vm_pmap;
	vm_offset_t	pend=0;
#define	ROUND_PAGE(x)	(((uint)(x) + PAGE_SIZE-1)& ~(PAGE_SIZE - 1))

 /* These are in phys memory */
	istack = ROUND_PAGE((uint)Sysmap + SYSPTSIZE * 4);
	(u_int)scratch = istack + ISTACK_SIZE;
	mtpr(scratch, PR_ISP); /* set interrupt stack pointer */
	msgbufp = (void *)(scratch + NBPG * 4);
	(u_int)pv_table = (int)ROUND_PAGE(sizeof(struct msgbuf)) +
	    (u_int)msgbufp;

/* Count up phys memory */
	while (!badaddr(pend, 4))
		pend += NBPG * 128;

#if VAX630
	if (cpu_type == VAX_630)
		pend -= 8 * NBPG;       /* Avoid console scratchpad */
#endif
#if VAX650
	if (cpu_type == VAX_650)
		pend -= 64 * NBPG;
#endif
/* These are virt only */
	vmmap = ROUND_PAGE(pv_table + (pend / PAGE_SIZE));
	(u_int)Numem = vmmap + NBPG * 2;

	(pt_entry_t *)UMEMmap=kvtopte(Numem);
	(pt_entry_t *)pte_cmap=kvtopte(vmmap);

	avail_start=ROUND_PAGE(vmmap)&0x7fffffff;
	avail_end=pend-ROUND_PAGE(sizeof(struct msgbuf));
	virtual_avail=ROUND_PAGE((uint)Numem+NUBA*NBPG*NBPG);
	virtual_end=SYSPTSIZE*NBPG+KERNBASE;
#ifdef DEBUG
	printf("Sysmap %x, istack %x, scratch %x\n",Sysmap,istack,scratch);
	printf("etext %x, edata %x, end %x, esym %x\n",
	    &etext,&edata, &end, esym);
	printf("SYSPTSIZE %x, USRPTSIZE %x\n",SYSPTSIZE,USRPTSIZE);
	printf("pv_table %x, vmmap %x, Numem %x, pte_cmap %x\n",
		pv_table,vmmap,Numem,pte_cmap);
	printf("avail_start %x, avail_end %x\n",avail_start,avail_end);
	printf("virtual_avail %x,virtual_end %x\n",virtual_avail,virtual_end);
	printf("clearomr: %x \n",(uint)vmmap-(uint)Sysmap);
	printf("faultdebug %x, startsysc %x\n",&faultdebug, &startsysc);
	printf("startpmapdebug %x\n",&startpmapdebug);
#endif

	blkclr(Sysmap,(uint)vmmap-(uint)Sysmap);
	pmap_map(0x80000000,0,2*NBPG,VM_PROT_READ|VM_PROT_WRITE);
#ifdef DDB
	pmap_map(0x80000400,2*NBPG,(vm_offset_t)(&etext),
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);
#else
	pmap_map(0x80000400,2*NBPG,(vm_offset_t)(&etext),VM_PROT_EXECUTE);
#endif
	pmap_map((vm_offset_t)(&etext),(vm_offset_t)&etext,
		(vm_offset_t)Sysmap,VM_PROT_READ|VM_PROT_WRITE);
	pmap_map((vm_offset_t)Sysmap,(vm_offset_t)Sysmap,istack,
		VM_PROT_READ|VM_PROT_WRITE);
	pmap_map(istack,istack,istack+NBPG,VM_PROT_NONE);/* Red zone */
	pmap_map(istack+NBPG,istack+NBPG,(vm_offset_t)scratch,
		VM_PROT_READ|VM_PROT_WRITE);
	pmap_map((vm_offset_t)scratch,(vm_offset_t)scratch,
		(vm_offset_t)msgbufp, VM_PROT_READ|VM_PROT_WRITE);
        pmap_map((vm_offset_t)msgbufp, (vm_offset_t)msgbufp,
	    (vm_offset_t)pv_table, VM_PROT_ALL);
	pmap_map((vm_offset_t)pv_table,(vm_offset_t)pv_table,vmmap,
		VM_PROT_READ|VM_PROT_WRITE);

	/* Init kernel pmap */
	pmap_kernel()->ref_count = 1;
	simple_lock_init(&pmap_kernel()->pm_lock);
	p0pmap->pm_pcb=(struct pcb *)proc0paddr;

		 /* used for signal trampoline code */
	sigsida=(u_int)(scratch+NBPG)&0x7fffffff;
	bcopy(&sigcode, (void *)sigsida, (u_int)&esigcode-(u_int)&sigcode);

	p0pmap->pm_pcb->P1BR = (void *)0x80000000;
	p0pmap->pm_pcb->P0BR = 0;
	p0pmap->pm_pcb->P1LR = 0x200000;
	p0pmap->pm_pcb->P0LR = AST_PCB;
	mtpr(0x80000000, PR_P1BR);
	mtpr(0, PR_P0BR);
	mtpr(0x200000, PR_P1LR);
	mtpr(AST_PCB, PR_P0LR);
/*
 * Now everything should be complete, start virtual memory.
 */
	mtpr((uint)Sysmap&0x7fffffff,PR_SBR); /* Where is SPT? */
	mtpr(SYSPTSIZE,PR_SLR);
	mtpr(1,PR_MAPEN);
	bzero(valueptr, 200);
}

/****************************************************************************** *
 * pmap_init()
 *
 ******************************************************************************
 *
 * Called as part of vm init.
 *
 */

void 
pmap_init(s, e) 
	vm_offset_t s,e;
{

	/* reserve place on SPT for UPT */
	pte_map = kmem_suballoc(kernel_map, &ptemapstart, &ptemapend, 
	    USRPTSIZE * 4, TRUE);
}

/******************************************************************************
 *
 * pmap_create()
 *
 ******************************************************************************
 *
 * pmap_t pmap_create(phys_size)
 *
 * Create a pmap for a new task.
 * 
 * Allocate a pmap form kernel memory with malloc.
 * Clear the pmap.
 * Allocate a ptab for the pmap.
 * 
 */
pmap_t 
pmap_create(phys_size)
	vm_size_t phys_size;
{
	pmap_t   pmap;

#ifdef DEBUG
if(startpmapdebug)printf("pmap_create: phys_size %x\n",phys_size);
#endif
	if(phys_size) return NULL;

/* Malloc place for pmap struct */

	pmap = (pmap_t) malloc(sizeof(struct pmap), M_VMPMAP, M_WAITOK);
	pmap_pinit(pmap); 

	return (pmap);
}


/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap)
	struct pmap *pmap;
{
#ifdef DEBUG
if(startpmapdebug)printf("pmap_release: pmap %x\n",pmap);
#endif

	if (pmap->pm_pcb->P0BR)
		kmem_free_wakeup(pte_map, (vm_offset_t)pmap->pm_pcb->P0BR, 
		    (pmap->pm_pcb->P0LR & ~AST_MASK) * 4);

	if (pmap->pm_pcb->P1BR)
		kmem_free_wakeup(pte_map, (vm_offset_t)pmap->pm_stack,
		    (0x200000 - pmap->pm_pcb->P1LR) * 4);

	bzero(pmap, sizeof(struct pmap));
}


/*
 * pmap_destroy(pmap): Remove a reference from the pmap. 
 * If the pmap is NULL then just return else decrese pm_count.
 * If this was the last reference we call's pmap_relaese to release this pmap.
 * OBS! remember to set pm_lock
 */

void
pmap_destroy(pmap)
	pmap_t pmap;
{
	int count;
  
#ifdef DEBUG
if(startpmapdebug)printf("pmap_destroy: pmap %x\n",pmap);
#endif
	if (pmap == NULL)
		return;

	simple_lock(&pmap->pm_lock);
	count = --pmap->ref_count;
	simple_unlock(&pmap->pm_lock);
  
	if (!count) {
		pmap_release(pmap);
		free((caddr_t)pmap, M_VMPMAP);
	}
}

void 
pmap_enter(pmap, v, p, prot, wired)
	register pmap_t pmap;
	vm_offset_t     v;
	vm_offset_t     p;
	vm_prot_t       prot;
	boolean_t       wired;
{
	u_int j, i, pte, s, *patch;
	pv_entry_t pv, tmp;

	if (v > 0x7fffffff) pte = kernel_prot[prot] | PG_PFNUM(p) | PG_V;
	else pte = prot_array[prot] | PG_PFNUM(p) | PG_V;
	s = splimp();
	pv = PHYS_TO_PV(p);

#ifdef DEBUG
if(startpmapdebug)
printf("pmap_enter: pmap: %x,virt %x, phys %x,pv %x prot %x\n",
	pmap,v,p,pv,prot);
#endif
	if (!pmap) return;
	if (wired) pte |= PG_W;


	if (v < 0x40000000) {
		patch = (int *)pmap->pm_pcb->P0BR;
		i = (v >> PG_SHIFT);
		if (i >= (pmap->pm_pcb->P0LR&~AST_MASK))
			pmap_expandp0(pmap, i);
		patch = (int *)pmap->pm_pcb->P0BR;
	} else if (v < (u_int)0x80000000) {
		patch = (int *)pmap->pm_pcb->P1BR;
		i = (v - 0x40000000) >> PG_SHIFT;
		if (i < pmap->pm_pcb->P1LR)
			panic("pmap_enter: must expand P1");
	} else {
		patch = (int *)Sysmap;
		i = (v - (u_int)0x80000000) >> PG_SHIFT;
	}

	if ((patch[i] & PG_FRAME) == (pte & PG_FRAME)) { /* no map change */
		if ((patch[i] & PG_W) != (pte & PG_W)) { /* wiring change */
			pmap_change_wiring(pmap, v, wired);
		} else if ((patch[i] & PG_PROT) != (pte & PG_PROT)) {
			patch[i] &= ~PG_PROT;
			patch[i++] |= prot_array[prot];
			patch[i] &= ~PG_PROT;
			patch[i] |= prot_array[prot];
			mtpr(v, PR_TBIS);
			mtpr(v + NBPG, PR_TBIS);
		} else if ((patch[i] & PG_V) == 0) {
			if (patch[i] & PG_SREF) {
				patch[i] &= ~PG_SREF;
				patch[i] |= PG_V | PG_REF;
			} else patch[i] |= PG_V;
			if (patch[++i] & PG_SREF) {
				patch[i] &= ~PG_SREF;
				patch[i] |= PG_V | PG_REF;
			} else patch[i] |= PG_V;
			mtpr(v, PR_TBIS);
			mtpr(v + NBPG, PR_TBIS);
		} /* else nothing to do */
		splx(s);
		return;
	}

	if (!pv->pv_pmap) {
		pv->pv_pmap = pmap;
		pv->pv_next = NULL;
		pv->pv_va = v;
	} else {
		tmp = alloc_pv_entry();
		tmp->pv_pmap = pmap;
		tmp->pv_next = pv->pv_next;
		tmp->pv_va = v;
		pv->pv_next = tmp;
	}
	patch[i++] = pte++;
	patch[i] = pte;
	mtpr(v, PR_TBIS);
	mtpr(v + NBPG, PR_TBIS);
	splx(s);
}

void *
pmap_bootstrap_alloc(size)
	int size;
{
	void *mem;

	size = round_page(size);
	mem = (void *)virtual_avail;
	virtual_avail = pmap_map(virtual_avail, avail_start,
	    avail_start + size, VM_PROT_READ|VM_PROT_WRITE);
	avail_start += size;
	blkclr(mem, size);
	return (mem);
}

vm_offset_t
pmap_map(virtuell, pstart, pend, prot)
	vm_offset_t virtuell, pstart, pend;
	int prot;
{
	vm_offset_t count;
	int *pentry;

#ifdef DEBUG
if(startpmapdebug)printf("pmap_map: virt %x, pstart %x, pend %x\n",virtuell, pstart, pend);
#endif

	pstart=(uint)pstart &0x7fffffff;
	pend=(uint)pend &0x7fffffff;
	virtuell=(uint)virtuell &0x7fffffff;
	(uint)pentry= (((uint)(virtuell)>>PGSHIFT)*4)+(uint)Sysmap;
	for(count=pstart;count<pend;count+=NBPG){
		*pentry++ = (count>>PGSHIFT)|kernel_prot[prot]|PG_V;
	}
	mtpr(0,PR_TBIA);
	return(virtuell+(count-pstart)+0x80000000);
}

vm_offset_t 
pmap_extract(pmap, va)
	pmap_t pmap;
	vm_offset_t va;
{

	int	*pte, nypte;
#ifdef DEBUG
if(startpmapdebug)printf("pmap_extract: pmap %x, va %x\n",pmap, va);
#endif

	pte=(int *)pmap_virt2pte(pmap,va);
	if(pte) return(((*pte&PG_FRAME)<<PG_SHIFT)+((u_int)va&PGOFSET));
	else return 0;
}

/*
 * pmap_protect( pmap, vstart, vend, protect)
 */
void
pmap_protect(pmap, start, end, prot)
	pmap_t pmap;
	vm_offset_t start;
	vm_offset_t     end;
	vm_prot_t       prot;
{
	int pte, *patch, s;

#ifdef DEBUG
if(startpmapdebug) printf("pmap_protect: pmap %x, start %x, end %x, prot %x\n",
	pmap, start, end,prot);
#endif
	if(pmap==NULL) return;
	s=splimp();
	if(start>0x7fffffff) pte=kernel_prot[prot];
	else pte=prot_array[prot];

	if(end<0x40000000){
		while((end>>PG_SHIFT)>(pmap->pm_pcb->P0LR&~AST_MASK))
			pmap_expandp0(pmap,(end>>PG_SHIFT));
	} else if(end<(u_int)0x80000000){
		u_int i;
		i=(start&0x3fffffff)>>PG_SHIFT;
		if(i<pmap->pm_pcb->P1LR)
			start=((pmap->pm_pcb->P1LR)<<PG_SHIFT)+0x40000000;
		i=(end&0x3fffffff)>>PG_SHIFT;
		if(i<pmap->pm_pcb->P1LR) return;
	}
	while (start < end) {
		patch = (int *)pmap_virt2pte(pmap,start);
		if(patch){
			*patch&=(~PG_PROT);
			*patch|=pte;
			mtpr(start,PR_TBIS);
		}
		start += NBPG;
	}
	mtpr(0,PR_TBIA);
	splx(s);
}

/*
 * pmap_remove(pmap, start, slut) removes all valid mappings between
 * the two virtual adresses start and slut from pmap pmap.
 * NOTE: all adresses between start and slut may not be mapped.
 */

void
pmap_remove(pmap, start, slut)
	pmap_t	pmap;
	vm_offset_t	start, slut;
{
	u_int *ptestart, *pteslut,i,s,*temp;
	pv_entry_t	pv;
	vm_offset_t	countup;

#ifdef DEBUG
if(startpmapdebug) printf("pmap_remove: pmap=0x %x, start=0x %x, slut=0x %x\n",
	   pmap, start, slut);
#endif

	if(!pmap) return;
	if(!pmap->pm_pcb&&start<0x80000000) return; /* No page registers */
/* First, get pte first address */
	if(start<0x40000000){ /* P0 */
		if(!(temp=pmap->pm_pcb->P0BR)) return; /* No page table */
		ptestart=&temp[start>>PG_SHIFT];
		pteslut=&temp[slut>>PG_SHIFT];
		if(pteslut>&temp[(pmap->pm_pcb->P0LR&~AST_MASK)])
			pteslut=&temp[(pmap->pm_pcb->P0LR&~AST_MASK)];
	} else if(start>0x7fffffff){ /* System region */
		ptestart=(u_int *)&Sysmap[(start&0x3fffffff)>>PG_SHIFT];
		pteslut=(u_int *)&Sysmap[(slut&0x3fffffff)>>PG_SHIFT];
	} else { /* P1 (stack) region */
		if(!(temp=pmap->pm_pcb->P1BR)) return; /* No page table */
		pteslut=&temp[(slut&0x3fffffff)>>PG_SHIFT];
		ptestart=&temp[(start&0x3fffffff)>>PG_SHIFT];
		if(ptestart<&temp[pmap->pm_pcb->P1LR])
			ptestart=&temp[pmap->pm_pcb->P1LR];
	}

#ifdef DEBUG
if(startpmapdebug)
printf("pmap_remove: ptestart %x, pteslut %x, pv %x\n",ptestart, pteslut,pv);
#endif

	s=splimp();
	for(countup=start;ptestart<pteslut;ptestart+=2, countup+=PAGE_SIZE){

		if(!(*ptestart&PG_FRAME))
			continue; /* not valid phys addr,no mapping */

		pv=PTE_TO_PV(*ptestart);
		if(!remove_pmap_from_mapping(pv,pmap)){
			panic("pmap_remove: pmap not in pv_table");
		}
		*ptestart=0;
		*(ptestart+1)=0;
	}
	mtpr(0,PR_TBIA);
	splx(s);
}


remove_pmap_from_mapping(pv, pmap)
	pv_entry_t pv;
	pmap_t	pmap;
{
	pv_entry_t temp_pv,temp2;

	if(!pv->pv_pmap&&pv->pv_next)
		panic("remove_pmap_from_mapping: j{ttefel");

	if(pv->pv_pmap==pmap){
		if(pv->pv_next){
			temp_pv=pv->pv_next;
			pv->pv_pmap=temp_pv->pv_pmap;
			pv->pv_va=temp_pv->pv_va;
			pv->pv_next=temp_pv->pv_next;
			free_pv_entry(temp_pv);
		} else {
			bzero(pv,sizeof(struct pv_entry));
		}
	} else {
		temp_pv=pv;
		while(temp_pv->pv_next){
			if(temp_pv->pv_next->pv_pmap==pmap){
				temp2=temp_pv->pv_next;
				temp_pv->pv_next=temp2->pv_next;
				free_pv_entry(temp2);
				return 1;
			}
			temp_pv=temp_pv->pv_next;
		}
		return 0;
	}
	return 1;
}

void 
pmap_copy_page(src, dst)
	vm_offset_t   src;
	vm_offset_t   dst;
{
	int s;
	extern uint vmmap;

#ifdef DEBUG
if(startpmapdebug)printf("pmap_copy_page: src %x, dst %x\n",src, dst);
#endif
	s=splimp();
	pte_cmap[0]=(src>>PGSHIFT)|PG_V|PG_RO;
	pte_cmap[1]=(dst>>PGSHIFT)|PG_V|PG_KW;
	mtpr(vmmap,PR_TBIS);
	mtpr(vmmap+NBPG,PR_TBIS);
	bcopy((void *)vmmap, (void *)vmmap+NBPG, NBPG);
	pte_cmap[0]=((src+NBPG)>>PGSHIFT)|PG_V|PG_RO;
	pte_cmap[1]=((dst+NBPG)>>PGSHIFT)|PG_V|PG_RW;
	mtpr(vmmap,PR_TBIS);
	mtpr(vmmap+NBPG,PR_TBIS);
	bcopy((void *)vmmap, (void *)vmmap+NBPG, NBPG);
	splx(s);
}

pv_entry_t 
alloc_pv_entry()
{
	pv_entry_t temporary;

	if(!pv_head) {
		temporary=(pv_entry_t)malloc(sizeof(struct pv_entry),
			M_VMPVENT, M_NOWAIT);
#ifdef DEBUG
if(startpmapdebug) printf("alloc_pv_entry: %x\n",temporary);
#endif
	} else {
		temporary=pv_head;
		pv_head=temporary->pv_next;
		pv_count--;
	}
	bzero(temporary, sizeof(struct pv_entry));
	return temporary;
}

void
free_pv_entry(entry)
	pv_entry_t entry;
{
	if(pv_count>=100) {	 /* Should be a define? */
		free(entry, M_VMPVENT);
	} else {
		entry->pv_next=pv_head;
		pv_head=entry;
		pv_count++;
	}
}

boolean_t
pmap_is_referenced(pa)
	vm_offset_t     pa;
{
	struct pv_entry *pv;
	u_int *pte,spte=0;

	pv=PHYS_TO_PV(pa);

	if(!pv->pv_pmap) return 0;

	do {
		pte=(u_int *)pmap_virt2pte(pv->pv_pmap,pv->pv_va);
		spte|=*pte++;
		spte|=*pte;
	} while(pv=pv->pv_next);
	return((spte&PG_REF)?1:0);
}

boolean_t
pmap_is_modified(pa)
     vm_offset_t     pa;
{
	struct pv_entry *pv;
	u_int *pte, spte=0;

	pv=PHYS_TO_PV(pa);
	if(!pv->pv_pmap) return 0;
	do {
                pte=(u_int *)pmap_virt2pte(pv->pv_pmap,pv->pv_va);
                spte|=*pte++;
                spte|=*pte;
	} while(pv=pv->pv_next);
	return((spte&PG_M)?1:0);
}

/*
 * Reference bits are simulated and connected to logical pages,
 * not physical. This makes reference simulation much easier.
 */

void 
pmap_clear_reference(pa)
	vm_offset_t     pa;
{
	struct pv_entry *pv;
	int *pte,s,i;
/*
 * Simulate page reference bit
 */
	pv=PHYS_TO_PV(pa);
#ifdef DEBUG
if(startpmapdebug) printf("pmap_clear_reference: pa %x, pv %x\n",pa,pv);
#endif

	pv->pv_flags&=~PV_REF;
	if(!pv->pv_pmap) return;

	do {
		pte=(int *)pmap_virt2pte(pv->pv_pmap,pv->pv_va);
		*pte&= ~(PG_REF|PG_V);
		*pte++|=PG_SREF;
		*pte&= ~(PG_REF|PG_V);
		*pte|=PG_SREF;
	} while(pv=pv->pv_next);
	mtpr(0,PR_TBIA);
}

void 
pmap_clear_modify(pa)
	vm_offset_t     pa;
{
	struct pv_entry *pv;
	u_int *pte,spte=0,s;

	pv=PHYS_TO_PV(pa);
	if(!pv->pv_pmap) return;
	do {
		pte=(u_int *)pmap_virt2pte(pv->pv_pmap,pv->pv_va);
		*pte++&= ~PG_M;
		*pte&= ~PG_M;
	} while(pv=pv->pv_next);
}

void 
pmap_change_wiring(pmap, va, wired)
	register pmap_t pmap;
	vm_offset_t     va;
	boolean_t       wired;
{
	int *pte;
#ifdef DEBUG
if(startpmapdebug) printf("pmap_change_wiring: pmap %x, va %x, wired %x\n",
	pmap, va, wired);
#endif

	pte=(int *)pmap_virt2pte(pmap,va);
	if(!pte) return; /* no pte allocated */
	if(wired) *pte|=PG_W;
	else *pte&=~PG_W;
}

/*
 *      pmap_page_protect:
 *
 *      Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(pa, prot)
	vm_offset_t     pa;
	vm_prot_t       prot;
{
	pv_entry_t pv,opv;
	u_int s,*pte,*pte1,nyprot,kprot;
  
#ifdef DEBUG
if(startpmapdebug) printf("pmap_page_protect: pa %x, prot %x\n",pa, prot);
#endif
	pv = PHYS_TO_PV(pa);
	if(!pv->pv_pmap) return;
	nyprot=prot_array[prot];
	kprot=kernel_prot[prot];

	switch (prot) {

	case VM_PROT_ALL:
		break;
	case VM_PROT_READ:
	case VM_PROT_READ|VM_PROT_EXECUTE:
		do {
			pte=pte1=(int *)pmap_virt2pte(pv->pv_pmap, pv->pv_va);
			s=splimp();
			*pte1++&=~PG_PROT;
			*pte1&=~PG_PROT;
			if(pv->pv_va>0x7fffffff){
				*pte|=kprot;
				*pte1|=kprot;
			} else{
				*pte|=nyprot;
				*pte1|=nyprot;
			}
			splx(s);
		} while(pv=pv->pv_next);
		mtpr(0,PR_TBIA);
		break;

	default:

		pte=(int *)pmap_virt2pte(pv->pv_pmap, pv->pv_va);
		s = splimp();
		*pte++=0;
		*pte=0;
		opv=pv;
		pv=pv->pv_next;
		bzero(opv,sizeof(struct pv_entry));
		while(pv){
			pte=(int *)pmap_virt2pte(pv->pv_pmap, pv->pv_va);
			*pte++=0;
			*pte=0;
			opv=pv;
			pv=pv->pv_next;
			free_pv_entry(opv);
		}

		mtpr(0,PR_TBIA);
		splx(s);
		break;
	}
}

/*
 *      pmap_zero_page zeros the specified (machine independent)
 *      page by mapping the page into virtual memory and using
 *      bzero to clear its contents, one machine dependent page
 *      at a time.
 */
void
pmap_zero_page(phys)
	vm_offset_t    phys;
{
	int s;

#ifdef DEBUG
if(startpmapdebug)printf("pmap_zero_page(phys %x, vmmap %x, pte_cmap %x\n",
	phys,vmmap,pte_cmap);
#endif
	s=splimp();
	pte_cmap[0]=(phys>>PG_SHIFT)|PG_V|PG_KW;
	pte_cmap[1]=pte_cmap[0]+1;
	mtpr(vmmap,PR_TBIS);
	mtpr(vmmap+NBPG,PR_TBIS);
	bzero((void *)vmmap,NBPG*2);
	pte_cmap[0]=pte_cmap[1]=0;
	mtpr(vmmap,PR_TBIS);
	mtpr(vmmap+NBPG,PR_TBIS);
	splx(s);
}

pt_entry_t *
pmap_virt2pte(pmap,vaddr)
	pmap_t	pmap;
	u_int	vaddr;
{
	u_int *pte,scr;

	if(vaddr<0x40000000){
		pte=pmap->pm_pcb->P0BR;
		if((vaddr>>PG_SHIFT)>(pmap->pm_pcb->P0LR&~AST_MASK)) return 0;
	} else if(vaddr<(u_int)0x80000000){
		pte=pmap->pm_pcb->P1BR;
		if(((vaddr&0x3fffffff)>>PG_SHIFT)<pmap->pm_pcb->P1LR) return 0;
	} else {
		pte=(u_int *)Sysmap;
	}

	vaddr&=(u_int)0x3fffffff;

	return((pt_entry_t *)&pte[vaddr>>PG_SHIFT]);
}

pmap_expandp0(pmap,ny_storlek)
	struct pmap *pmap;
{
	u_int tmp,s,size,osize,oaddr,astlvl,*i,j;

	astlvl=pmap->pm_pcb->P0LR&AST_MASK;
	osize=(pmap->pm_pcb->P0LR&~AST_MASK)*4;
	size=ny_storlek*4;
	tmp=kmem_alloc_wait(pte_map, size);
	s=splhigh();
	if(osize) blkcpy(pmap->pm_pcb->P0BR, (void*)tmp,osize);
	oaddr=(u_int)pmap->pm_pcb->P0BR;
	mtpr(tmp,PR_P0BR);
	mtpr(((size>>2)|astlvl),PR_P0LR);
	mtpr(0,PR_TBIA);
	pmap->pm_pcb->P0BR=(void*)tmp;
	pmap->pm_pcb->P0LR=((size>>2)|astlvl);
	splx(s);
	if(osize)
		kmem_free_wakeup(pte_map, (vm_offset_t)oaddr, osize);
}

pmap_expandp1(pmap)
	struct pmap *pmap;
{
	u_int tmp,s,size,osize,oaddr,*i,j;

	osize=0x800000-(pmap->pm_pcb->P1LR*4);
	size=osize+PAGE_SIZE;
	tmp=kmem_alloc_wait(pte_map, size);
	s=splhigh();

	if(osize) blkcpy((void*)pmap->pm_stack, (void*)tmp+PAGE_SIZE,osize);
	oaddr=pmap->pm_stack;
	pmap->pm_pcb->P1BR=(void*)(tmp+size-0x800000);
	pmap->pm_pcb->P1LR=(0x800000-size)>>2;
	pmap->pm_stack=tmp;
	mtpr(pmap->pm_pcb->P1BR,PR_P1BR);
	mtpr(pmap->pm_pcb->P1LR,PR_P1LR);
	mtpr(0,PR_TBIA);
	splx(s);
	if (osize)
		kmem_free_wakeup(pte_map, (vm_offset_t)oaddr, osize);
}
