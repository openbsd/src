/*	$NetBSD: db_interface.c,v 1.3 1995/04/10 13:15:43 mycroft Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * Copyright (c) 1992 Helsinki University of Technology
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON AND HELSINKI UNIVERSITY OF TECHNOLOGY ALLOW FREE USE
 * OF THIS SOFTWARE IN ITS "AS IS" CONDITION.  CARNEGIE MELLON AND
 * HELSINKI UNIVERSITY OF TECHNOLOGY DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon 
 * the rights to redistribute these changes.
 */
/*
 * 	File: ns532/db_interface.c
 *	Author: Tero Kivinen, Helsinki University of Technology 1992.
 *
 *	Interface to new kernel debugger.
 */

#include <sys/reboot.h>
#include <vm/pmap.h>

#include <ns532/thread.h>
#include <ns532/db_machdep.h>
#include <ns532/trap.h>
#include <ns532/setjmp.h>
#include <ns532/machparam.h>
#include <mach/vm_param.h>
#include <vm/vm_map.h>
#include <kern/thread.h>
#include <kern/task.h>
#include <ddb/db_task_thread.h>

int	db_active = 0;

/*
 * Received keyboard interrupt sequence.
 */
void
kdb_kbd_trap(regs)
	struct ns532_saved_state *regs;
{
	if (db_active == 0) {
		printf("\n\nkernel: keyboard interrupt\n");
		kdb_trap(-1, 0, regs);
	}
}

extern char *	trap_type[];
extern int	TRAP_TYPES;

/*
 * Print trap reason.
 */
void
kdbprinttrap(type, code)
	int	type, code;
{
	printf("kernel: ");
	if (type > TRAP_TYPES)
	    printf("type %d", type);
	else
	    printf("%s", trap_type[type]);
	printf(" trap, code=%x\n", code);
}

/*
 *  kdb_trap - field a TRACE or BPT trap
 */

extern jmp_buf_t *db_recover;

int db_active_ipl;

kdb_trap(type, code, regs)
	int	type, code;
	register struct ns532_saved_state *regs;
{
	int s;

	s = splsched();
	db_active_ipl = s;
	
	switch (type) {
	      case T_BPT:		/* breakpoint */
	      case T_WATCHPOINT:	/* watchpoint */
	      case T_TRC:		/* trace step */
	      case T_DBG:		/* hardware debug trap */
	      case -1:	/* keyboard interrupt */
		break;
		
	      default:
		if (db_recover) {
			db_printf("Caught ");
			if (type > TRAP_TYPES)
			    db_printf("type %d", type);
			else
			    db_printf("%s", trap_type[type]);
			db_printf(" trap, code = %x, pc = %x\n",
				  code, regs->pc);
			db_error("");
			/*NOTREACHED*/
		}
		kdbprinttrap(type, code);
	}
	
	/*  Should switch to kdb's own stack here. */
	
	ddb_regs = *regs;
	
	db_active++;
	cnpollc(TRUE);
	db_task_trap(type, code, (DDB_REGS->psr & PSR_U) == 0);
	cnpollc(FALSE);
	db_active--;
	
	if ((type = T_BPT) &&
	    (db_get_task_value(PC_REGS(DDB_REGS), BKPT_SIZE, FALSE, TASK_NULL)
	     == BKPT_INST))
	    PC_REGS(DDB_REGS) += BKPT_SIZE;
	
	*regs = ddb_regs;
	(void) splx(s);
	return (1);
}

int
db_user_to_kernel_address(task, addr, kaddr, flag)
	task_t		task;
	vm_offset_t	addr;
	unsigned	*kaddr;
	int		flag;
{
	register pt_entry_t *ptp;
	
	ptp = pmap_pte(task->map->pmap, addr);
	if (ptp == PT_ENTRY_NULL || (*ptp & NS532_PTE_VALID) == 0) {
		if (flag) {
			db_printf("\nno memory is assigned to address %08x\n",
				  addr);
			db_error(0);
			/* NOTREACHED */
		}
		return(-1);
	}
	*kaddr = (unsigned)ptetokv(*ptp) + (addr & (NS532_PGBYTES-1));
	return(0);
}
	
/*
 * Read bytes from kernel address space for debugger.
 */

void
db_read_bytes(addr, size, data, task)
	vm_offset_t	addr;
	register int	size;
	register char	*data;
	task_t		task;
{
	register char	*src;
	register int	n;
	unsigned	kern_addr;
	
	src = (char *)addr;
	if (addr >= VM_MIN_KERNEL_ADDRESS || task == TASK_NULL) {
		if (task == TASK_NULL)
		    task = db_current_task();
		while (--size >= 0) {
			if (addr++ < VM_MIN_KERNEL_ADDRESS &&
			    task == TASK_NULL) {
				db_printf("\nbad address %x\n", addr);
				db_error(0);
				/* NOTREACHED */
			}
			*data++ = *src++;
		}
		return;
	}
	while (size > 0) {
		if (db_user_to_kernel_address(task, addr, &kern_addr, 1) < 0)
		    return;
		src = (char *)kern_addr;
		n = ns532_trunc_page(addr + NS532_PGBYTES) - addr;
		if (n > size)
		    n = size;
		size -= n;
		addr += n;
		while (--n >= 0)
		    *data++ = *src++;
	}
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(addr, size, data, task)
	vm_offset_t	addr;
	register int	size;
	register char	*data;
	task_t		task;
{
	register char	*dst;
	
	register pt_entry_t *ptep0 = 0;
	pt_entry_t	oldmap0 = 0;
	vm_offset_t	addr1;
	register pt_entry_t *ptep1 = 0;
	pt_entry_t	oldmap1 = 0;
	extern char	etext;
	void		db_write_bytes_user_space();
	
	if ((addr < VM_MIN_KERNEL_ADDRESS) ^
	    ((addr + size) <= VM_MIN_KERNEL_ADDRESS)) {
		db_error("\ncannot write data into mixed space\n");
		/* NOTREACHED */
	}
	if (addr < VM_MIN_KERNEL_ADDRESS) {
		if (task) {
			db_write_bytes_user_space(addr, size, data, task);
			return;
		} else if (db_current_task() == TASK_NULL) {
			db_printf("\nbad address %x\n", addr);
			db_error(0);
			/* NOTREACHED */
		}
	}
	
	if (addr >= VM_MIN_KERNEL_ADDRESS &&
	    addr <= (vm_offset_t)&etext)
	{
		ptep0 = pmap_pte(pmap_kernel(), addr);
		oldmap0 = *ptep0;
		*ptep0 |= NS532_PTE_WRITE;
		
		addr1 = ns532_trunc_page(addr + size - 1);
		if (ns532_trunc_page(addr) != addr1) {
			/* data crosses a page boundary */
			
			ptep1 = pmap_pte(pmap_kernel(), addr1);
			oldmap1 = *ptep1;
			*ptep1 |= NS532_PTE_WRITE;
		}
		_flush_tlb();
	}
	
	dst = (char *)addr;
	
	while (--size >= 0) {
		*dst++ = *data++;
		_flush_instruction_cache_addr(dst);
	}
	
	if (ptep0) {
		*ptep0 = oldmap0;
		if (ptep1) {
			*ptep1 = oldmap1;
		}
		_flush_tlb();
	}
}

void
db_write_bytes_user_space(addr, size, data, task)
	vm_offset_t	addr;
	register int	size;
	register char	*data;
	task_t		task;
{
	register char	*dst;
	register	n;
	unsigned	kern_addr;
	
	while (size > 0) {
		if (db_user_to_kernel_address(task, addr, &kern_addr, 1) < 0)
		    return;
		dst = (char *)kern_addr;
		n = ns532_trunc_page(addr+NS532_PGBYTES) - addr;
		if (n > size)
		    n = size;
		size -= n;
		addr += n;
		while (--n >= 0)
		    *dst++ = *data++;
	}
}

boolean_t
db_check_access(addr, size, task)
	vm_offset_t	addr;
	register int	size;
	task_t		task;
{
	register	n;
	unsigned	kern_addr;
	
	if (addr >= VM_MIN_KERNEL_ADDRESS) {
		if (kernel_task == TASK_NULL)
		    return(TRUE);
		task = kernel_task;
	} else if (task == TASK_NULL) {
		if (current_thread() == THREAD_NULL)
		    return(FALSE);
		task = current_thread()->task;
	}
	while (size > 0) {
		if (db_user_to_kernel_address(task, addr, &kern_addr, 0) < 0)
		    return(FALSE);
		n = ns532_trunc_page(addr+NS532_PGBYTES) - addr;
		if (n > size)
		    n = size;
		size -= n;
		addr += n;
	}
	return(TRUE);
}

boolean_t
db_phys_eq(task1, addr1, task2, addr2)
	task_t		task1;
	vm_offset_t	addr1;
	task_t		task2;
	vm_offset_t	addr2;
{
	unsigned	kern_addr1, kern_addr2;
	
	if (addr1 >= VM_MIN_KERNEL_ADDRESS || addr2 >= VM_MIN_KERNEL_ADDRESS)
	    return(FALSE);
	if ((addr1 & (NS532_PGBYTES-1)) != (addr2 & (NS532_PGBYTES-1)))
	    return(FALSE);
	if (task1 == TASK_NULL) {
		if (current_thread() == THREAD_NULL)
		    return(FALSE);
		task1 = current_thread()->task;
	}
	if (db_user_to_kernel_address(task1, addr1, &kern_addr1, 0) < 0
	    || db_user_to_kernel_address(task2, addr2, &kern_addr2) < 0)
	    return(FALSE);
	return(kern_addr1 == kern_addr2);
}

#define DB_USER_STACK_ADDR		(VM_MIN_KERNEL_ADDRESS)
#define DB_NAME_SEARCH_LIMIT		(DB_USER_STACK_ADDR-(NS532_PGBYTES*3))

static int
db_search_null(task, svaddr, evaddr, skaddr, flag)
	task_t		task;
	unsigned	*svaddr;
	unsigned	evaddr;
	unsigned	*skaddr;
	int		flag;
{
	register unsigned vaddr;
	register unsigned *kaddr;
	
	kaddr = (unsigned *)*skaddr;
	for (vaddr = *svaddr; vaddr > evaddr; vaddr -= sizeof(unsigned)) {
		if (vaddr % NS532_PGBYTES == 0) {
			vaddr -= sizeof(unsigned);
			if (db_user_to_kernel_address(task, vaddr,
						      skaddr, 0) < 0)
			    return(-1);
			kaddr = (unsigned *)*skaddr;
		} else {
			vaddr -= sizeof(unsigned);
			kaddr--;
		}
		if ((*kaddr == 0) ^ (flag  == 0)) {
			*svaddr = vaddr;
			*skaddr = (unsigned)kaddr;
			return(0);
		}
	}
	return(-1);
}

void
db_task_name(task)
	task_t		task;
{
	register char *p;
	register n;
	unsigned vaddr, kaddr;
	
	vaddr = DB_USER_STACK_ADDR;
	kaddr = 0;
	
	/*
	 * skip nulls at the end
	 */
	if (db_search_null(task, &vaddr,
			   DB_NAME_SEARCH_LIMIT, &kaddr, 0) < 0) {
		db_printf(DB_NULL_TASK_NAME);
		return;
	}
	/*
	 * search start of args
	 */
	if (db_search_null(task, &vaddr,
			   DB_NAME_SEARCH_LIMIT, &kaddr, 1) < 0) {
		db_printf(DB_NULL_TASK_NAME);
		return;
	}
	
	n = DB_TASK_NAME_LEN-1;
	p = (char *)kaddr + sizeof(unsigned);
	for (vaddr += sizeof(int); vaddr < DB_USER_STACK_ADDR && n > 0; 
	     vaddr++, p++, n--) {
		if (vaddr % NS532_PGBYTES == 0) {
			(void)db_user_to_kernel_address(task,
							vaddr, &kaddr, 0);
			p = (char*)kaddr;
		}
		db_printf("%c", (*p < ' ' || *p > '~')? ' ': *p);
	}
	while (n-- >= 0)	/* compare with >= 0 for one more space */
	    db_printf(" ");
}
