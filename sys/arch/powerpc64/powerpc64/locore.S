/*	$OpenBSD: locore.S,v 1.37 2020/09/25 17:31:27 kettenis Exp $	*/

/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "assym.h"

#include <sys/errno.h>
#include <sys/syscall.h>

#include <machine/param.h>
#include <machine/opal.h>
#include <machine/psl.h>

	.abiversion 2

	.rodata

	.globl sigcode
	.globl sigcoderet
sigcode:
	addi	%r1, %r1, -32
	mtctr	%r12
	bctrl
	addi	%r3, %r1, 32+SF_SC
	li	%r0, SYS_sigreturn
	sc
sigcoderet:
	li	%r0, SYS_exit
	sc
	.globl esigcode
esigcode:

	.globl sigfill
sigfill:
	.long	0
esigfill:
	.globl sigfillsiz
sigfillsiz:
	.long	esigfill - sigfill

	.text

#ifdef MULTIPROCESSOR

	.globl cpu_hatch
cpu_hatch:
	bl	1f
1:	mflr	%r31
	addis	%r2, %r31, (.TOC. - 1b)@ha
	addi	%r2, %r2, (.TOC. - 1b)@l

	addis	%r1, %r2, tmpstack_end@toc@ha
	addi	%r1, %r1, tmpstack_end@toc@l
	addi	%r1, %r1, -32

	bl	cpu_bootstrap

	mfsprg0	%r31
	ld	%r1, CI_INITSTACK_END(%r31)
	addi	%r1, %r1, -32

	bl	cpu_start_secondary
	b	.

	.data

	.align PAGE_SHIFT
tmpstack:
	.space PAGE_SIZE
tmpstack_end:

	.text

#endif

	.globl cpu_idle_enter
cpu_idle_enter:
	blr

	.globl cpu_idle_cycle
cpu_idle_cycle:
	blr

	.globl cpu_idle_leave
cpu_idle_leave:
	blr

	.globl cpu_switchto_asm
cpu_switchto_asm:
	cmpdi	%r3, 0
	beq	1f

	mflr	%r0
	std	%r0, 16(%r1)
	mfcr	%r10
	stdu	%r1, -SFRAMELEN(%r1)
	std	%r10, SF_CR(%r1)
	std	%r14, SF_R14(%r1)
	std	%r15, SF_R15(%r1)
	std	%r16, SF_R16(%r1)
	std	%r17, SF_R17(%r1)
	std	%r18, SF_R18(%r1)
	std	%r19, SF_R19(%r1)
	std	%r20, SF_R20(%r1)
	std	%r21, SF_R21(%r1)
	std	%r22, SF_R22(%r1)
	std	%r23, SF_R23(%r1)
	std	%r24, SF_R24(%r1)
	std	%r25, SF_R25(%r1)
	std	%r26, SF_R26(%r1)
	std	%r27, SF_R27(%r1)
	std	%r28, SF_R28(%r1)
	std	%r29, SF_R29(%r1)
	std	%r30, SF_R30(%r1)
	std	%r31, SF_R31(%r1)

	ld	%r31, P_ADDR(%r3)
	std	%r1, PCB_SP(%r31)

1:
	mfsprg0	%r30

	li	%r31, SONPROC
	stb	%r31, P_STAT(%r4)
	std	%r4, CI_CURPROC(%r30)

	ld	%r31, P_ADDR(%r4)
	std	%r31, CI_CURPCB(%r30)
	ld	%r1, PCB_SP(%r31)
#if PCB_SP != 0
	addi	%r31, %r31, PCB_SP
#endif
	stdcx.	%r1, 0, %r31	/* clear a possible reservation */

	ld	%r31, P_MD_USER_SLB_PA(%r4)
	std	%r31, CI_USER_SLB_PA(%r30)

	ld	%r31, SF_R31(%r1)
	ld	%r30, SF_R30(%r1)
	ld	%r29, SF_R29(%r1)
	ld	%r28, SF_R28(%r1)
	ld	%r27, SF_R27(%r1)
	ld	%r26, SF_R26(%r1)
	ld	%r25, SF_R25(%r1)
	ld	%r24, SF_R24(%r1)
	ld	%r23, SF_R23(%r1)
	ld	%r22, SF_R22(%r1)
	ld	%r21, SF_R21(%r1)
	ld	%r20, SF_R20(%r1)
	ld	%r19, SF_R19(%r1)
	ld	%r18, SF_R18(%r1)
	ld	%r17, SF_R17(%r1)
	ld	%r16, SF_R16(%r1)
	ld	%r15, SF_R15(%r1)
	ld	%r14, SF_R14(%r1)
	ld	%r10, SF_CR(%r1)
	addi	%r1, %r1, SFRAMELEN
	mtcr	%r10
	ld	%r0, 16(%r1)
	mtlr	%r0
	blr

	.globl kcopy
kcopy:
	mfsprg0	%r7
	ld	%r7, CI_CURPCB(%r7)
	ld	%r8, PCB_ONFAULT(%r7)
	addis	%r9, %r2, .Lkcopyfault@toc@ha
	addi	%r9, %r9, .Lkcopyfault@toc@l
	std	%r9, PCB_ONFAULT(%r7)

	rldicl.	%r6, %r5, 62, 2
	beq	0, .Lkcopybytes

	addi	%r3, %r3, -4
	addi	%r4, %r4, -4
	mtctr	%r6
1:
	lwzu	%r6, 4(%r3)
	stwu	%r6, 4(%r4)
	bdnz	1b

	addi	%r3, %r3, 4
	addi	%r4, %r4, 4

.Lkcopybytes:
	andi.	%r6, %r5, 3
	beq	0, .Lkcopysuccess

	addi	%r3, %r3, -1
	addi	%r4, %r4, -1
	mtctr	%r6
2:
	lbzu	%r6, 1(%r3)
	stbu	%r6, 1(%r4)
	bdnz	2b

.Lkcopysuccess:
	std	%r8, PCB_ONFAULT(%r7)
	li	%r3, 0
	blr

.Lkcopyfault:
	std	%r8, PCB_ONFAULT(%r7)
	li	%r3, EFAULT
	blr

	.globl copystr
copystr:
	mfsprg0	%r7
	ld	%r7, CI_CURPCB(%r7)
	ld	%r8, PCB_ONFAULT(%r7)
	addis	%r9, %r2, .Lcopystrfault@toc@ha
	addi	%r9, %r9, .Lcopystrfault@toc@l
	std	%r9, PCB_ONFAULT(%r7)

	cmpdi	%r5, 0
	beq	.Lcopystrtoolong

	addi	%r3, %r3, -1
	addi	%r4, %r4, -1
	mtctr	%r5
	li	%r9, 0
1:
	addi	%r9, %r9, 1
	lbzu	%r5, 1(%r3)
	stbu	%r5, 1(%r4)
	cmplwi	%r5, 0
	beq	.Lcopystrsuccess
	bdnz	1b

.Lcopystrtoolong:
	li	%r3, ENAMETOOLONG
	b	.Lcopystrcleanup

.Lcopystrfault:
	li	%r3, EFAULT
	b	.Lcopystrcleanup

.Lcopystrsuccess:
	li	%r3, 0

.Lcopystrcleanup:
	cmpldi	%r6, 0
	beq	2f
	std	%r9, 0(%r6)
2:
	std	%r8, PCB_ONFAULT(%r7)
	blr

	.globl proc_trampoline
	.type proc_trampoline, @function
proc_trampoline:
#ifdef MULTIPROCESSOR
	bl	proc_trampoline_mp
#endif
	li	%r3, IPL_NONE
	bl	splx
	mr	%r12, %r31
	mr	%r3, %r30
	mtctr	%r12
	bctrl
	b	trapexit

/*
 * OPAL interfaces
 */

	.data

	.globl opal_base
opal_base:
	.quad	0
	.globl opal_entry
opal_entry:
	.quad	0

	.text

#define OPAL_CALL(opal_token, name) \
	.globl name;			\
name:					\
	li	%r0, opal_token;	\
	b	opal_call

OPAL_CALL(OPAL_TEST, opal_test)
OPAL_CALL(OPAL_CONSOLE_WRITE, opal_console_write)
OPAL_CALL(OPAL_CONSOLE_READ, opal_console_read)
OPAL_CALL(OPAL_RTC_READ, opal_rtc_read)
OPAL_CALL(OPAL_RTC_WRITE, opal_rtc_write)
OPAL_CALL(OPAL_CEC_POWER_DOWN, opal_cec_power_down)
OPAL_CALL(OPAL_CEC_REBOOT, opal_cec_reboot)
OPAL_CALL(OPAL_HANDLE_INTERRUPT, opal_handle_interrupt)
OPAL_CALL(OPAL_POLL_EVENTS, opal_poll_events)
OPAL_CALL(OPAL_PCI_CONFIG_READ_WORD, opal_pci_config_read_word)
OPAL_CALL(OPAL_PCI_CONFIG_WRITE_WORD, opal_pci_config_write_word)
OPAL_CALL(OPAL_SET_XIVE, opal_set_xive)
OPAL_CALL(OPAL_GET_XIVE, opal_get_xive)
OPAL_CALL(OPAL_PCI_EEH_FREEZE_STATUS, opal_pci_eeh_freeze_status)
OPAL_CALL(OPAL_PCI_EEH_FREEZE_CLEAR, opal_pci_eeh_freeze_clear)
OPAL_CALL(OPAL_PCI_PHB_MMIO_ENABLE, opal_pci_phb_mmio_enable)
OPAL_CALL(OPAL_PCI_SET_PHB_MEM_WINDOW, opal_pci_set_phb_mem_window)
OPAL_CALL(OPAL_PCI_MAP_PE_MMIO_WINDOW, opal_pci_map_pe_mmio_window)
OPAL_CALL(OPAL_PCI_SET_PE, opal_pci_set_pe)
OPAL_CALL(OPAL_PCI_SET_XIVE_PE, opal_pci_set_xive_pe)
OPAL_CALL(OPAL_GET_MSI_32, opal_get_msi_32)
OPAL_CALL(OPAL_GET_MSI_64, opal_get_msi_64)
OPAL_CALL(OPAL_START_CPU, opal_start_cpu)
OPAL_CALL(OPAL_PCI_MAP_PE_DMA_WINDOW_REAL, opal_pci_map_pe_dma_window_real)
OPAL_CALL(OPAL_PCI_RESET, opal_pci_reset)
OPAL_CALL(OPAL_REINIT_CPUS, opal_reinit_cpus)
OPAL_CALL(OPAL_CHECK_TOKEN, opal_check_token)
OPAL_CALL(OPAL_SENSOR_READ, opal_sensor_read)
OPAL_CALL(OPAL_CONSOLE_FLUSH, opal_console_flush)
OPAL_CALL(OPAL_XIVE_RESET, opal_xive_reset)
OPAL_CALL(OPAL_XIVE_GET_IRQ_INFO, opal_xive_get_irq_info)
OPAL_CALL(OPAL_XIVE_GET_IRQ_CONFIG, opal_xive_get_irq_config)
OPAL_CALL(OPAL_XIVE_SET_IRQ_CONFIG, opal_xive_set_irq_config)
OPAL_CALL(OPAL_XIVE_GET_QUEUE_INFO, opal_xive_get_queue_info)
OPAL_CALL(OPAL_XIVE_SET_QUEUE_INFO, opal_xive_set_queue_info)
OPAL_CALL(OPAL_XIVE_GET_VP_INFO, opal_xive_get_vp_info)
OPAL_CALL(OPAL_XIVE_SET_VP_INFO, opal_xive_set_vp_info)
OPAL_CALL(OPAL_XIVE_DUMP, opal_xive_dump)
OPAL_CALL(OPAL_SENSOR_READ_U64, opal_sensor_read_u64)

opal_call:
	mflr	%r11
	std	%r11, 16(%r1)
	stdu	%r1, -32(%r1)
	std	%r2, 24(%r1)

	addis	%r11, %r2, opal_base@toc@ha
	addi	%r11, %r11, opal_base@toc@l
	addis	%r12, %r2, opal_entry@toc@ha
	addi	%r12, %r12, opal_entry@toc@l
	ld	%r11, 0(%r11)
	ld	%r12, 0(%r12)

	/* Save MSR */
	std	%r31, 12(%r1)
	mfmsr	%r31

	/* Disable translation and external interrupts */
	andi.	%r2, %r31, ~(PSL_DR|PSL_IR|PSL_EE)@l
	mtmsr	%r2
	isync

	mr	%r2, %r11
	mtctr	%r12
	bctrl

	/* Restore MSR */
	mtmsrd	%r31
	isync
	ld	%r31, 12(%r1)

	ld	%r2, 24(%r1)
	addi	%r1, %r1, 32
	ld	%r0, 16(%r1)
	mtlr	%r0
	blr

/*
 * DDB support code
 */

#ifdef DDB
	.globl db_enter
db_enter:
	trap
	/* The trap handler will return from db_enter(). */

	.globl setjmp
setjmp:
	mflr	%r0
	mfcr	%r5
	std	%r0, 0x00(%r3)	/* lr */
	std	%r5, 0x08(%r3)	/* cr */
	std	%r1, 0x10(%r3)
	std	%r2, 0x18(%r3)
	std	%r14, 0x20(%r3)
	std	%r15, 0x28(%r3)
	std	%r16, 0x30(%r3)
	std	%r17, 0x38(%r3)
	std	%r18, 0x40(%r3)
	std	%r19, 0x48(%r3)
	std	%r20, 0x50(%r3)
	std	%r21, 0x58(%r3)
	std	%r22, 0x60(%r3)
	std	%r23, 0x68(%r3)
	std	%r24, 0x70(%r3)
	std	%r25, 0x78(%r3)
	std	%r26, 0x80(%r3)
	std	%r27, 0x88(%r3)
	std	%r28, 0x90(%r3)
	std	%r29, 0x98(%r3)
	std	%r30, 0xa0(%r3)
	std	%r31, 0xa8(%r3)
	li	%r3, 0		/* return 0 */
	blr

	.globl longjmp
longjmp:
	ld	%r0, 0x00(%r3)	/* lr */
	ld	%r5, 0x08(%r3)	/* cr */
	mtlr	%r0
	mtcr	%r5
	ld	%r1, 0x10(%r3)
	ld	%r2, 0x18(%r3)
	ld	%r14, 0x20(%r3)
	ld	%r15, 0x28(%r3)
	ld	%r16, 0x30(%r3)
	ld	%r17, 0x38(%r3)
	ld	%r18, 0x40(%r3)
	ld	%r19, 0x48(%r3)
	ld	%r20, 0x50(%r3)
	ld	%r21, 0x58(%r3)
	ld	%r22, 0x60(%r3)
	ld	%r23, 0x68(%r3)
	ld	%r24, 0x70(%r3)
	ld	%r25, 0x78(%r3)
	ld	%r26, 0x80(%r3)
	ld	%r27, 0x88(%r3)
	ld	%r28, 0x90(%r3)
	ld	%r29, 0x98(%r3)
	ld	%r30, 0xa0(%r3)
	ld	%r31, 0xa8(%r3)
	mr	%r4, %r3	/* return val */
	blr
#endif
