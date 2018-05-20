/*	$OpenBSD: frameasm.h,v 1.13 2018/04/26 12:47:02 guenther Exp $	*/
/*	$NetBSD: frameasm.h,v 1.1 2003/04/26 18:39:40 fvdl Exp $	*/

#ifndef _AMD64_MACHINE_FRAMEASM_H
#define _AMD64_MACHINE_FRAMEASM_H

/*
 * Macros to define pushing/popping frames for interrupts, traps
 * and system calls. Currently all the same; will diverge later.
 */

/*
 * These are used on interrupt or trap entry or exit.
 */
#define INTR_SAVE_GPRS \
	subq	$120,%rsp		; \
	INTR_SAVE_MOST_GPRS_NO_ADJ	; \
	movq	%rcx,TF_RCX(%rsp)
#define INTR_SAVE_MOST_GPRS_NO_ADJ \
	movq	%r15,TF_R15(%rsp)	; \
	movq	%r14,TF_R14(%rsp)	; \
	movq	%r13,TF_R13(%rsp)	; \
	movq	%r12,TF_R12(%rsp)	; \
	movq	%r11,TF_R11(%rsp)	; \
	movq	%r10,TF_R10(%rsp)	; \
	movq	%r9,TF_R9(%rsp)		; \
	movq	%r8,TF_R8(%rsp)		; \
	movq	%rdi,TF_RDI(%rsp)	; \
	movq	%rsi,TF_RSI(%rsp)	; \
	movq	%rbp,TF_RBP(%rsp)	; \
	leaq	TF_RBP(%rsp),%rbp	; \
	movq	%rbx,TF_RBX(%rsp)	; \
	movq	%rdx,TF_RDX(%rsp)	; \
	movq	%rax,TF_RAX(%rsp)

/* For real interrupt code paths, where we can come from userspace */
#define INTRENTRY_LABEL(label)	X##label##_untramp
#define	INTRENTRY(label) \
	testq	$SEL_RPL,24(%rsp)	; \
	je	INTRENTRY_LABEL(label)	; \
	swapgs				; \
	movq	%rax,CPUVAR(SCRATCH)	; \
	movq	CPUVAR(KERN_CR3),%rax	; \
	testq	%rax,%rax		; \
	jz	98f			; \
	movq	%rax,%cr3		; \
	jmp	98f			; \
	.text				; \
	.global	INTRENTRY_LABEL(label)	; \
INTRENTRY_LABEL(label):	/* from kernel */ \
	subq	$152,%rsp		; \
	movq	%rcx,TF_RCX(%rsp)	; \
	/* the hardware puts err next to %rip, we move it elsewhere and */ \
	/* later put %rbp in this slot to make it look like a call frame */ \
	movq	(TF_RIP - 8)(%rsp),%rcx	; \
	movq	%rcx,TF_ERR(%rsp)	; \
	jmp	99f			; \
98:	/* from userspace */		  \
	movq	CPUVAR(KERN_RSP),%rax	; \
	xchgq	%rax,%rsp		; \
	movq	%rcx,TF_RCX(%rsp)	; \
	/* copy trapno+err to the trap frame */ \
	movq	0(%rax),%rcx		; \
	movq	%rcx,TF_TRAPNO(%rsp)	; \
	movq	8(%rax),%rcx		; \
	movq	%rcx,TF_ERR(%rsp)	; \
	addq	$16,%rax		; \
	/* copy iretq frame to the trap frame */ \
	movq	IRETQ_RIP(%rax),%rcx	; \
	movq	%rcx,TF_RIP(%rsp)	; \
	movq	IRETQ_CS(%rax),%rcx	; \
	movq	%rcx,TF_CS(%rsp)	; \
	movq	IRETQ_RFLAGS(%rax),%rcx	; \
	movq	%rcx,TF_RFLAGS(%rsp)	; \
	movq	IRETQ_RSP(%rax),%rcx	; \
	movq	%rcx,TF_RSP(%rsp)	; \
	movq	IRETQ_SS(%rax),%rcx	; \
	movq	%rcx,TF_SS(%rsp)	; \
	movq	CPUVAR(SCRATCH),%rax	; \
99:	INTR_SAVE_MOST_GPRS_NO_ADJ

/* For faking up an interrupt frame when we're already in the kernel */
#define	INTR_REENTRY \
	subq	$32,%rsp		; \
	INTR_SAVE_GPRS

#define INTRFASTEXIT \
	jmp	intr_fast_exit

#define INTR_RECURSE_HWFRAME \
	movq	%rsp,%r10		; \
	movl	%ss,%r11d		; \
	pushq	%r11			; \
	pushq	%r10			; \
	pushfq				; \
	movl	%cs,%r11d		; \
	pushq	%r11			; \
	pushq	%r13			;

#define	INTR_FAKE_TRAP	0xbadabada

#define CHECK_ASTPENDING(reg)	movq	CPUVAR(CURPROC),reg		; \
				cmpq	$0, reg				; \
				je	99f				; \
				cmpl	$0, P_MD_ASTPENDING(reg)	; \
				99:

#define CLEAR_ASTPENDING(reg)	movl	$0, P_MD_ASTPENDING(reg)

#endif /* _AMD64_MACHINE_FRAMEASM_H */
