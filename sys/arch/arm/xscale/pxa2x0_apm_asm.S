/*	$OpenBSD: pxa2x0_apm_asm.S,v 1.3 2007/10/08 20:18:19 miod Exp $	*/

/*
 * Copyright (c) 2005 Uwe Stuehler <uwe@openbsd.org>
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

#include <machine/asm.h>
#include <machine/cpu.h>

#include <arch/arm/xscale/pxa2x0reg.h>
#include <arch/arm/sa11x0/sa11x0_reg.h>

/* XXX replace with values defined elsewhere. */
#define DCACHE_CACHELINECOUNT	1024
#define CACHELINESIZE		32

/* cp14 register 6 */
#define CLKCFG_T		(1<<0)	/* turbo */
#define CLKCFG_F		(1<<1)	/* frequency change */
#define CLKCFG_HT		(1<<2)	/* half-turbo */
#define CLKCFG_B		(1<<3)	/* fast-bus */

/* cp14 register 7 */
#define PWRMODE_NORMAL		(0<<0)
#define PWRMODE_IDLE		(1<<0)
#define PWRMODE_STANDBY		(2<<0)
#define PWRMODE_SLEEP		(3<<0)
#define PWRMODE_DEEP_SLEEP	(7<<0)

/* XXX */
#define MDREFR_C3000		(MDREFR_K0DB2|MDREFR_E1PIN|MDREFR_K1RUN|\
				 MDREFR_K1DB2|MDREFR_K2DB2|MDREFR_APD)
#define MDREFR_DRI_91MHZ	(0x13<<0)
#define MDREFR_HIGH		(MDREFR_C3000 | 0x030)
#define MDREFR_LOW		(MDREFR_C3000 | 0x00b)
#define MDREFR_SPEED_91		(MDREFR_C3000 | MDREFR_DRI_91MHZ)
#define MDREFR_SPEED_LOW	(MDREFR_C3000 | 0x017)
#define MSC0_HIGH \
		( 7 << MSC_RRR_SHIFT << 16) | \
		(15 << MSC_RDN_SHIFT << 16) | \
		(15 << MSC_RDF_SHIFT << 16) | \
		(MSC_RT_NONBURST     << 16) | \
		( 2 << MSC_RRR_SHIFT)       | \
		(13 << MSC_RDN_SHIFT)       | \
		(13 << MSC_RDF_SHIFT)       | \
		MSC_RBW	/* PXA271 */        | \
		MSC_RT_NONBURST
#define MSC1_HIGH \
		( 7 << MSC_RRR_SHIFT << 16) | \
		(15 << MSC_RDN_SHIFT << 16) | \
		(15 << MSC_RDF_SHIFT << 16) | \
		(MSC_RT_VLIO         << 16) | \
		( 3 << MSC_RRR_SHIFT)       | \
		( 4 << MSC_RDN_SHIFT)       | \
		(13 << MSC_RDF_SHIFT)       | \
		MSC_RT_VLIO
#define MSC2_HIGH \
		( 7 << MSC_RRR_SHIFT << 16) | \
		(15 << MSC_RDN_SHIFT << 16) | \
		(15 << MSC_RDF_SHIFT << 16) | \
		(MSC_RT_NONBURST     << 16) | \
		( 3 << MSC_RRR_SHIFT)       | \
		( 4 << MSC_RDN_SHIFT)       | \
		(13 << MSC_RDF_SHIFT)       | \
		MSC_RT_VLIO
#define MSC0_LOW \
		( 7 << MSC_RRR_SHIFT << 16) | \
		(15 << MSC_RDN_SHIFT << 16) | \
		(15 << MSC_RDF_SHIFT << 16) | \
		(MSC_RT_NONBURST     << 16) | \
		( 1 << MSC_RRR_SHIFT)       | \
		( 8 << MSC_RDN_SHIFT)       | \
		( 8 << MSC_RDF_SHIFT)       | \
		MSC_RBW	/* PXA271 */        | \
		MSC_RT_NONBURST
#define MSC1_LOW \
		( 7 << MSC_RRR_SHIFT << 16) | \
		(15 << MSC_RDN_SHIFT << 16) | \
		(15 << MSC_RDF_SHIFT << 16) | \
		(MSC_RT_VLIO         << 16) | \
		( 1 << MSC_RRR_SHIFT)       | \
		( 2 << MSC_RDN_SHIFT)       | \
		( 6 << MSC_RDF_SHIFT)       | \
		MSC_RT_VLIO
#define MSC2_LOW \
		( 7 << MSC_RRR_SHIFT << 16) | \
		(15 << MSC_RDN_SHIFT << 16) | \
		(15 << MSC_RDF_SHIFT << 16) | \
		(MSC_RT_NONBURST     << 16) | \
		( 1 << MSC_RRR_SHIFT)       | \
		( 2 << MSC_RDN_SHIFT)       | \
		( 6 << MSC_RDF_SHIFT)       | \
		MSC_RT_VLIO

	.text
	.global	_C_LABEL(vector_page)
	.global	_C_LABEL(xscale_cache_clean_addr)
	.global	_C_LABEL(pxa2x0_clkman_ioh)
	.global	_C_LABEL(pxa2x0_memctl_ioh)

.Lvector_page:
	.word	_C_LABEL(vector_page)
.Lxscale_cache_clean_addr:
	.word	_C_LABEL(xscale_cache_clean_addr)

.Lgpioiohp:	.word	_C_LABEL(pxa2x0_gpio_ioh)
.Lclkmaniohp:	.word	_C_LABEL(pxa2x0_clkman_ioh)
.Lmemctliohp:	.word	_C_LABEL(pxa2x0_memctl_ioh)

.Lsleepdata:	.word	sleepdata
.Lsleepdata_phys: .word	sleepdata - 0xc0200000 + 0xa0200000 /* XXX */
.Lsleepdata_svc: .word	sleepdata_svc

.Lcccr_high:	.word	CCCR_A | CCCR_TURBO_X2 | CCCR_RUN_X16
.Lmdrefr_high:	.word	MDREFR_HIGH
.Lmsc0_high:	.word	MSC0_HIGH
.Lmsc1_high:	.word	MSC1_HIGH
.Lmsc2_high:	.word	MSC2_HIGH
.Lmdrefr_low:	.word	MDREFR_LOW
.Lmsc0_low:	.word	MSC0_LOW
.Lmsc1_low:	.word	MSC1_LOW
.Lmsc2_low:	.word	MSC2_LOW

/*
 * void pxa2x0_cpu_suspend(void)
 *
 * Enter sleep mode without automatic voltage change.  The core must
 * be in low power mode, and interrupts disabled.
 */
ENTRY(pxa2x0_cpu_suspend)
	stmdb	sp!, {r0-r12, lr}

	ldr	r3, .Lsleepdata		/* Point to the data area. */
	ldr	r2, =pxa2x0_cpu_resume_virt
	str	r2, [r3], #4
	
	mrc	p15, 0, r2, c1, c0, 0	/* Load MMU control register. */
	mov	r0,     #0xff000000
	orr	r0, r0, #0x00ff0000
	bic	r2, r2, r0		/* Clear undefined bits. */
	str	r2, [r3], #4		/* Save MMU control register. */

	mrc	p15, 0, r2, c2, c0, 0	/* Load TTB address. */
	mov	r0,     #0x00003f00
	orr	r0, r0, #0x000000ff
	bic	r2, r2, r0		/* Clear undefined bits. */
	str	r2, [r3], #4		/* Save TTB address. */

	mrc	p15, 0, r2, c3, c0, 0	/* Load domain access control. */
	str	r2, [r3], #4		/* Save domain access control. */

	mrs	r2, spsr		/* Load SVC saved CPSR. */
	str	r2, [r3], #4		/* Save SVC saved CPSR. */
	str     sp, [r3], #4		/* Save SVC stack pointer. */

	mov	r1, #(PSR_FIQ32_MODE | I32_bit | F32_bit)
	msr	cpsr, r1		/* Enter FIQ mode. */
	mrs     r2, spsr		/* Load FIQ mode saved CPSR. */
	stmia   r3!, {r2, r8-r12, sp, lr} /* Save FIQ mode registers. */

	mov	r1, #(PSR_IRQ32_MODE | I32_bit | F32_bit)
	msr	cpsr, r1		/* Enter IRQ mode. */
	mrs     r0, spsr		/* Load IRQ mode saved CPSR. */
	stmia   r3!, {r0, sp, lr}	/* Save IRQ mode registers. */

	mov	r1, #(PSR_ABT32_MODE | I32_bit | F32_bit)
	msr	cpsr, r1		/* Enter ABT mode. */
	mrs     r0, spsr		/* Load ABT mode saved CPSR. */
	stmia   r3!, {r0, sp, lr}	/* Save ABT mode registers. */

	mov	r1, #(PSR_UND32_MODE | I32_bit | F32_bit)
	msr	cpsr, r1		/* Enter UND mode. */
	mrs     r0, spsr		/* Load UND mode saved CPSR. */
	stmia   r3!, {r0, sp, lr}	/* Save UND mode registers. */

	mov	r1, #(PSR_SYS32_MODE | I32_bit | F32_bit)
	msr	cpsr, r1		/* Enter SYS mode. */
	stmia   r3!, {sp, lr}		/* Save SYS mode registers. */

	mov	r1, #(PSR_SVC32_MODE | I32_bit | F32_bit)
	msr	cpsr, r1		/* Return to SVC mode. */

	/* At this point all critical registers have been saved. */

	mov	r0, #0
	mcr	p15, 0, r0, c7, c10, 4	/* XXX does exactly what? */

	mov     r1, #DCACHE_CACHELINECOUNT
	ldr	r0, .Lxscale_cache_clean_addr

cache_flush_loop:
	mrs	r2, cpsr
	orr	r2, r2, #(I32_bit|F32_bit)
	msr	cpsr_c, r2		/* disable IRQ/FIQ */

	mcr	p15, 0, r0, c7, c2, 5	/* allocate cache line */
	mcr	p15, 0, r0, c7, c6, 1	/* flush D cache single entry */

	mrs	r2, cpsr
	and	r2, r2, #~(I32_bit|F32_bit)
	msr	cpsr_c, r2		/* enable IRQ/FIQ */

	add	r0, r0, #CACHELINESIZE
	subs	r1, r1, #1
	bne	cache_flush_loop

	mov	r0, #0
	mcr	p15, 0, r0, c7, c10, 4  /* drain write buffer */

	b	1f
1:
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

	/* Prepare to enter sleep mode. */
	mov	r1, #PWRMODE_SLEEP

	/* Prepare to put SDRAM into self-refresh mode. */
	ldr	r4, .Lmemctliohp
	ldr	r4, [r4]
	add	r4, r4, #MEMCTL_MDREFR
	ldr	r5, [r4]
	orr	r5, r5, #MDREFR_SLFRSH

	/* XXX prepare pointer to physical address 0, but for whom? */
	ldr	r2, .Lvector_page
	
	/*
	 * Execute the rest of this routine from cache.  The needed values
	 * are now in registers.
	 */
	b	1f
	/* XXX tell as(1) to dump the literal pool here, but why? */
	.ltorg
	.align	5
1:

	/* Put SDRAM into self-refresh mode manually. */
	str	r5, [r4]
	nop
	
	/*
	 * Enter sleep mode.  Exit from sleep mode returns the processor
	 * to normal run mode.  Execution resumes at the physical address
	 * stored in the PSPR after the required boot sequence (a short
	 * excursion into the ROM boot loader).
	 */
	mcr	p14, 0, r1, c7, c0, 0

	/* Just in case that wake-up does not resume at */
	nop
	nop
	nop
1:
	b	1b

/*
 * void pxa2x0_cpu_resume(void)
 */
	.align 5
ENTRY(pxa2x0_cpu_resume)
	/* XXX C3000-specific */
	ldr	r0, .Lmdrefr_addr_phys
	b	1f
	.align 5
1:
	ldr	r2, [r0]
	bic	r2, r2, #MDREFR_DRI & 0x000000ff
	bic	r2, r2, #MDREFR_DRI & 0x0000ff00
	orr	r2, r2, #MDREFR_DRI_91MHZ
	str	r2, [r0]
	b	1f
	.align 5
1:
	ldr	r0, .Lsleepdata_phys	/* Point to PA of saved data. */

	ldmia	r0!, {r7-r10}
	mcr	p15, 0, r10, c3, c0, 0	/* Restore domain access control. */
	mcr	p15, 0, r9, c2, c0, 0	/* Restore TTB address. */
	mcr	p15, 0, r0, c8, c7, 0	/* Flush I+D TLBs. */
	mcr	p15, 0, r0, c7, c7, 0	/* Flush I+D BTB. */
	mcr	p15, 0, r8, c1, c0, 0	/* Restore MMU control. */
	mov	pc, r7			/* Jump to virtual address. */
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

pxa2x0_cpu_resume_virt:
	ldr	r2, .Lsleepdata_svc	/* Load VA of saved registers. */

	/* Restore SVC mode SPSR and stack pointer. */
	ldr	r0, [r2], #4
	msr	spsr, r0
	ldr	sp, [r2], #4

	/* Restore FIQ mode registers. */
	mov	r1, #(PSR_FIQ32_MODE | I32_bit | F32_bit)
	msr	cpsr, r1
	ldr	r0, [r2], #4
	msr	spsr, r0
	ldr	r8, [r2], #4
	ldr	r9, [r2], #4
	ldr	r10, [r2], #4
	ldr	r11, [r2], #4
	ldr	r12, [r2], #4
	ldr	sp, [r2], #4
	ldr	lr, [r2], #4

	/* Restore IRQ mode registers. */
	mov	r1, #(PSR_IRQ32_MODE | I32_bit | F32_bit)
	msr	cpsr, r1
	ldr	r0, [r2], #4
	msr	spsr, r0
	ldr	sp, [r2], #4
	ldr	lr, [r2], #4

	/* Restore ABT mode registers. */
	mov	r1, #(PSR_ABT32_MODE | I32_bit | F32_bit)
	msr	cpsr, r1
	ldr	r0, [r2], #4
	msr	spsr, r0
	ldr	sp, [r2], #4
	ldr	lr, [r2], #4

	/* Restore UND mode registers. */
	mov	r1, #(PSR_UND32_MODE | I32_bit | F32_bit)
	msr	cpsr, r1
	ldr	r0, [r2], #4
	msr	spsr, r0
	ldr	sp, [r2], #4
	ldr	lr, [r2], #4

	/* Restore SYS mode registers. */
	mov	r1, #(PSR_SYS32_MODE | I32_bit | F32_bit)
	msr	cpsr, r1
	ldr	sp, [r2], #4
	ldr	lr, [r2], #4

	/* Return to SVC mode. */
	mov	r1, #(PSR_SVC32_MODE | I32_bit | F32_bit)
	msr	cpsr, r1

	ldmia	sp!, {r0-r12, pc}

.Lmdrefr_addr_phys:
	.word	PXA2X0_MEMCTL_BASE + MEMCTL_MDREFR

	.data

/*
 * Saved processor state
 */
	.align	5
sleepdata:
	.word	0		/* =pxa2x0_cpu_resume_virt */
	.word	0		/* MMU control */
	.word	0		/* MMU TTB address */
	.word	0		/* MMU domain access control */
sleepdata_svc:
	.word	0		/* SVC mode saved CPSR */
	.word	0		/* SVC mode stack pointer */
	.word	0		/* FIQ mode saved CPSR */
	.word	0		/* FIQ mode r8 */
	.word	0		/* FIQ mode r9 */
	.word	0		/* FIQ mode r10 */
	.word	0		/* FIQ mode r11 */
	.word	0		/* FIQ mode r12 */
	.word	0		/* FIQ mode stack pointer */
	.word	0		/* FIQ mode link register */
	.word	0		/* IRQ mode saved CPSR */
	.word	0		/* IRQ mode stack pointer */
	.word	0		/* IRQ mode link register */
	.word	0		/* ABT mode saved CPSR */
	.word	0		/* ABT mode stack pointer */
	.word	0		/* ABT mode link register */
	.word	0		/* UND mode saved CPSR */
	.word	0		/* UND mode stack pointer */
	.word	0		/* UND mode link register */
	.word	0		/* SYS mode stack pointer */
	.word	0		/* SYS mode link register */

	.text

/*
 * void pxa27x_run_mode(void)
 *
 * Disable half-turbo and turbo mode, but keep fast-bus mode.
 * Memory and LCD clock is not changed, so no reconfiguration is
 * necessary.
 */
ENTRY(pxa27x_run_mode)
	stmdb	sp!, {r0}
	mrc	p14, 0, r0, c6, c0, 0
	and	r0, r0, #~(CLKCFG_HT | CLKCFG_F| CLKCFG_T)
	mcr	p14, 0, r0, c6, c0, 0
	ldmia	sp!, {r0}
	mov	pc, lr

/*
 * void pxa27x_fastbus_run_mode(int enable, u_int32_t mdrefr)
 *
 * Enter normal run mode with fast-bus mode enabled or disabled.
 * The new value of MDREFR is programmed before or after CLKCFG,
 * as appropriate.
 */
 	.align 5
ENTRY(pxa27x_fastbus_run_mode)
 	stmdb	sp!, {r0-r2, lr}
	ldr	r2, .Lmemctliohp
	ldr	r2, [r2]
	cmp	r0, #0
	beq	disable_fastbus
	b	enable_fastbus
	.align	5
enable_fastbus:
	/* Enter normal run mode with fast-bus mode enabled. */
	mov	r0, #CLKCFG_B
 	mcr	p14, 0, r0, c6, c0, 0
	/* Set the new SDRAM refresh rate. */
 	str	r1, [r2, #MEMCTL_MDREFR]
	ldr	r0, [r2, #MEMCTL_MDREFR]
	mov	r0, r0
	ldmia	sp!, {r0-r2, pc}
 	.align 5
disable_fastbus:
	/* Set the new SDRAM refresh rate. */
 	str	r1, [r2, #MEMCTL_MDREFR]
	ldr	r0, [r2, #MEMCTL_MDREFR]
	mov	r0, r0
	/* Enter normal run mode with fast-bus mode disabled. */
	mov	r0, #0x0
 	mcr	p14, 0, r0, c6, c0, 0
	ldmia	sp!, {r0-r2, pc}

/* Keep these offsets in sync with struct memcfg. */
#define memcfg_mdrefr_high	0x00
#define memcfg_mdrefr_low	0x04
#define memcfg_mdrefr_low2	0x08	/* unused */
#define memcfg_msc_high		0x0c
#define memcfg_msc_low		0x18
#define memcfg_mdrefr_91	0x24

/*
 * void pxa27x_frequency_change(int cccr, int clkcfg,
 *    struct pxa2x0_memcfg *memcfg)
 *
 * Change the core PLL frequency and SDRAM refresh rate, ensuring the
 * proper sequence of operations.  If the CCCR_A bit is clear and L
 * is not equal to 7 the result is undefined.
 */
	.align 5
ENTRY(pxa27x_frequency_change)
	stmdb	sp!, {r0-r5, lr}

	/* Always write to CCCR before a frequency change. */
	ldr	r3, .Lclkmaniohp
	ldr	r3, [r3]
	str	r0, [r3, #CLKMAN_CCCR]

	/* Load the needed values into registers to avoid SDRAM access. */
	and	r3, r0, #CCCR_L_MASK
	ldr	r0, .Lmemctliohp
	ldr	r0, [r0]
	cmp	r3, #CCCR_RUN_X7		/* L=7 is 91MHz mode */
	beq	frequency_change_91
	and	r3, r1, #CLKCFG_B
	cmp	r3, #CLKCFG_B
	bne	frequency_change_208
	/* FALLTHROUGH */
frequency_change_high:
	ldr	r3, [r2, #memcfg_mdrefr_low]
	ldr	r4, [r2, #memcfg_mdrefr_high]
	add	r2, r2, #memcfg_msc_high
	bl	frequency_change_on_cache	/* XXX why BL? */
frequency_change_208:
	ldr	r3, [r2, #memcfg_mdrefr_low]
	ldr	r4, [r2, #memcfg_mdrefr_low]
	add	r2, r2, #memcfg_msc_high
	bl	frequency_change_on_cache
frequency_change_91:
	ldr	r3, [r2, #memcfg_mdrefr_low]
	ldr	r4, [r2, #memcfg_mdrefr_91]
	add	r2, r2, #memcfg_msc_low
	bl	frequency_change_on_cache

	/* Align execution to a cache line. */
	.align 5
frequency_change_on_cache:
	/* Change to a low SDRAM refresh rate.  Wait until the store to
	 * MDREFR is complete, following section 2.4 I/O Ordering and
	 * 6.5.1.4 of the PXA27x Developer's Manual. */
	str	r3, [r0, #MEMCTL_MDREFR]
	ldr	r5, [r0, #MEMCTL_MDREFR]
	mov	r5, r5
	/* Program new CLKCFG value, starting a core PLL frequency change
	 * if CLKCFG_F is set. */
	mcr	p14, 0, r1, c6, c0, 0
	/* Change SDRAM clock frequency to 104MHz, and ensure that the
	 * store to MDREFR is complete before the next SDRAM access. */
	str	r4, [r0, #MEMCTL_MDREFR]
	ldr	r5, [r0, #MEMCTL_MDREFR]
	mov	r5, r5
	/* Configure synchronous, static, and VLIO interfaces.  */
	ldr	r1, [r2], #4
	str	r1, [r0, #MEMCTL_MSC0]
	ldr	r1, [r2], #4
	str	r1, [r0, #MEMCTL_MSC1]
	ldr	r1, [r2]
	str	r1, [r0, #MEMCTL_MSC2]
	ldmia	sp!, {r0-r5, pc}

/*
 * void pxa27x_cpu_speed_91(void)
 *
 * Switch core run frequency to 91 MHz.
 */
	.align 5
ENTRY(pxa27x_cpu_speed_91)
 	stmdb	sp!, {r0-r3, lr}

	ldr	r0, .Lclkmaniohp
	ldr	r0, [r0]
	ldr	r1, .Lcccr_91
 	str	r1, [r0, #CLKMAN_CCCR]

	ldr	r0, .Lmemctliohp
	ldr	r0, [r0]
	ldr	r2, .Lmdrefr_91
	ldr	r3, .Lmdrefr_low
	
 	bl	1f
 	.align 5
1:
 	str	r3, [r0, #MEMCTL_MDREFR]
	ldr	r3, [r0, #MEMCTL_MDREFR]
		
	mov	r1, #CLKCFG_F
 	mcr	p14, 0, r1, c6, c0, 0
 	str	r2, [r0, #MEMCTL_MDREFR]
	ldr	r2, [r0, #MEMCTL_MDREFR]

	ldr	r1, .Lmsc0_low
 	str	r1, [r0, #MEMCTL_MSC0]
 	ldr	r1, .Lmsc1_low
 	str	r1, [r0, #MEMCTL_MSC1]
 	ldr	r1, .Lmsc2_low
 	str	r1, [r0, #MEMCTL_MSC2]

	ldmia	sp!, {r0-r3, pc}

.Lcccr_91:	.word	CCCR_TURBO_X1 | CCCR_RUN_X7
.Lmdrefr_91:	.word	MDREFR_SPEED_91
