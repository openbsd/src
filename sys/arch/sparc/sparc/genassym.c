/*	$NetBSD: genassym.c,v 1.17.4.1 1996/06/12 20:31:21 pk Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)genassym.c	8.1 (Berkeley) 6/11/93
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>

#include <machine/pmap.h>
#include <machine/cpu.h>

#include <machine/oldmon.h>
#include <machine/bsd_openprom.h>

#ifdef notyet
#include <sparc/dev/zsreg.h>
#include <sparc/dev/zsvar.h>
#endif
#include <dev/ic/am7930reg.h>
#include <sparc/dev/amd7930var.h>

#include <sparc/dev/fdreg.h>
#include <sparc/dev/fdvar.h>

#include <stdio.h>
#include <stddef.h>

#define	off(what, str, mem) def(what, (int)offsetof(str, mem))

void def __P((char *, int));
void flush __P((void));
int main __P((void));

void
def(what, where)
	char *what;
	int where;
{

	if (printf("#define\t%s\t%d\n", what, where) < 0) {
		perror("printf");
		exit(1);
	}
}

void
flush()
{

	if (fflush(stdout)) {
		perror("fflush");
		exit(1);
	}
}

int
main()
{

	/* general constants */
	def("BSD", BSD);
	def("SUN4_PGSHIFT", SUN4_PGSHIFT);
	def("SUN4CM_PGSHIFT", SUN4CM_PGSHIFT);
	def("USRSTACK", USRSTACK);

	/* proc fields and values */
	off("P_ADDR", struct proc, p_addr);
	off("P_STAT", struct proc, p_stat);
	off("P_WCHAN", struct proc, p_wchan);
	off("P_VMSPACE", struct proc, p_vmspace);
	def("SRUN", SRUN);

	/* VM structure fields */
	off("VM_PMAP", struct vmspace, vm_pmap);
	off("VM_PMAP_CTX", struct vmspace, vm_pmap.pm_ctx);
	off("VM_PMAP_CTXNUM", struct vmspace, vm_pmap.pm_ctxnum);

	/* interrupt/fault metering */
	off("V_SWTCH", struct vmmeter, v_swtch);
	off("V_INTR", struct vmmeter, v_intr);
	off("V_FAULTS", struct vmmeter, v_faults);

	/* PTE bits and related information */
	def("PG_W", PG_W);
	def("PG_VSHIFT", PG_VSHIFT);
	def("PG_PROTSHIFT", PG_PROTSHIFT);
	def("PG_PROTUREAD", PG_PROTUREAD);
	def("PG_PROTUWRITE", PG_PROTUWRITE);
#if defined(SUN4M)
	def("SRMMU_TETYPE", SRMMU_TETYPE);
	def("SRMMU_TEPTE", SRMMU_TEPTE);
	def("SRMMU_PROT_MASK", SRMMU_PROT_MASK);
	def("PPROT_R_RW", PPROT_R_RW);
	def("PPROT_RX_RX", PPROT_RX_RX);
	def("PPROT_RWX_RWX", PPROT_RWX_RWX);
	def("PPROT_WRITE", PPROT_WRITE);
#endif

	/* FPU state */
	off("FS_REGS", struct fpstate, fs_regs);
	off("FS_FSR", struct fpstate, fs_fsr);
	off("FS_QSIZE", struct fpstate, fs_qsize);
	off("FS_QUEUE", struct fpstate, fs_queue);
	def("FSR_QNE", FSR_QNE);

	/* system calls */
	def("SYS_sigreturn", SYS_sigreturn);
	def("SYS_execve", SYS_execve);
	def("SYS_exit", SYS_exit);

	/* errno */
	def("EFAULT", EFAULT);
	def("ENAMETOOLONG", ENAMETOOLONG);

	/* PCB fields */
	off("PCB_NSAVED", struct pcb, pcb_nsaved);
	off("PCB_ONFAULT", struct pcb, pcb_onfault);
	off("PCB_PSR", struct pcb, pcb_psr);
	off("PCB_RW", struct pcb, pcb_rw);
	off("PCB_SP", struct pcb, pcb_sp);
	off("PCB_PC", struct pcb, pcb_pc);
	off("PCB_UW", struct pcb, pcb_uw);
	off("PCB_WIM", struct pcb, pcb_wim);

	/* interrupt enable register PTE */
	def("IE_REG_PTE_PG", PG_V | PG_W | PG_S | PG_NC | PG_OBIO);

#ifdef notyet
	/* ZSCC interrupt fields */
	off("ZSC_A", struct zs_softc, sc_a);
	off("ZSC_B", struct zs_softc, sc_b);
/*	off("ZL_WREG", struct zs_line, zl_wreg); */
	off("ZL_TBC", struct zs_line, zl_tbc);
	off("ZL_TBA", struct zs_line, zl_tba);
	off("ZL_RBPUT", struct zs_line, zl_rbput);
	off("ZL_RBUF", struct zs_line, zl_rbuf);
	def("ZSRR1_DO_bit", ffs(ZSRR1_DO) - 1);
#endif
	/* audio trap handler fields */
	off("AU_AMD", struct auio, au_amd);
	off("AU_RDATA", struct auio, au_rdata);
	off("AU_REND", struct auio, au_rend);
	off("AU_PDATA", struct auio, au_pdata);
	off("AU_PEND", struct auio, au_pend);
	off("AU_EVCNT", struct auio, au_intrcnt.ev_count);

	off("AMD_IR", struct amd7930, ir);
	off("AMD_BBRB", struct amd7930, bbrb);
	off("AMD_BBTB", struct amd7930, bbtb);

	def("PROM_BASE", PROM_BASE);

	off("PV_NODEOPS", struct promvec, pv_nodeops);
	off("PV_HALT", struct promvec, pv_halt);
	off("PV_EVAL", struct promvec, pv_fortheval.v0_eval);
	off("PV_ROMVEC_VERS", struct promvec, pv_romvec_vers);

	off("NO_NEXTNODE", struct nodeops, no_nextnode);
	off("NO_GETPROP", struct nodeops, no_getprop);

	off("OLDMON_PRINTF", struct om_vector, printf);
	off("OLDMON_HALT", struct om_vector, exitToMon);

	/* floppy trap handler fields */
	off("FDC_REG_MSR", struct fdcio, fdcio_reg_msr);
	off("FDC_REG_FIFO", struct fdcio, fdcio_reg_fifo);
	off("FDC_ISTATE", struct fdcio, fdcio_istate);
	off("FDC_STATUS", struct fdcio, fdcio_status);
	off("FDC_NSTAT", struct fdcio, fdcio_nstat);
	off("FDC_DATA", struct fdcio, fdcio_data);
	off("FDC_TC", struct fdcio, fdcio_tc);
	off("FDC_EVCNT", struct fdcio, fdcio_intrcnt.ev_count);

	flush();

	return(0);
}
