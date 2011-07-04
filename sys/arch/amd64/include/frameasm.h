/*	$OpenBSD: frameasm.h,v 1.6 2011/07/04 15:54:24 guenther Exp $	*/
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
	subq	$120,%rsp	; \
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
	movq	%rbx,TF_RBX(%rsp)	; \
	movq	%rdx,TF_RDX(%rsp)	; \
	movq	%rcx,TF_RCX(%rsp)	; \
	movq	%rax,TF_RAX(%rsp)

#define	INTRENTRY \
	subq	$32,%rsp		; \
	testq	$SEL_UPL,56(%rsp)	; \
	je	98f			; \
	swapgs				; \
	movw	%gs,0(%rsp)		; \
	movw	%fs,8(%rsp)		; \
	movw	%es,16(%rsp)		; \
	movw	%ds,24(%rsp)		; \
98: 	INTR_SAVE_GPRS

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

/*
 * Restore %ds, %es, %fs, and %gs, dealing with the FS.base MSR for
 * %fs and doing the cli/swapgs for %gs.  Uses %rax, %rcx, and %rdx
 */
#define INTR_RESTORE_SELECTORS						\
	movq	CPUVAR(CURPCB),%rdx	/* for below */			; \
	/* %es and %ds */						  \
	movw	TF_ES(%rsp),%es						; \
	movw	$(GSEL(GUDATA_SEL, SEL_UPL)),%ax			; \
	movw	%ax,%ds							; \
	/* Make sure both %fs and FS.base are the desired values */	  \
	movq	PCB_FSBASE(%rdx),%rax					; \
	cmpq	$0,%rax							; \
	jne	96f							; \
	movw	TF_FS(%rsp),%fs	/* zero FS.base by setting %fs */	; \
	jmp	98f							; \
96:	cmpq	CPUVAR(CUR_FSBASE),%rax					; \
	jne	97f							; \
	movw	%fs,%cx		/* FS.base same, how about %fs? */	; \
	cmpw	TF_FS(%rsp),%cx						; \
	je	99f							; \
97:	movw	TF_FS(%rsp),%fs		/* set them both */		; \
	movq	%rax,%rdx						; \
	shrq	$32,%rdx						; \
	movl	$MSR_FSBASE,%ecx					; \
	wrmsr								; \
98:	movq	%rax,CPUVAR(CUR_FSBASE)					; \
99:	cli		/* %fs done, so swapgs and do %gs */		; \
	swapgs								; \
	movw	TF_GS(%rsp),%gs


#define CHECK_ASTPENDING(reg)	movq	CPUVAR(CURPROC),reg		; \
				cmpq	$0, reg				; \
				je	99f				; \
				cmpl	$0, P_MD_ASTPENDING(reg)	; \
				99:

#define CLEAR_ASTPENDING(reg)	movl	$0, P_MD_ASTPENDING(reg)

#endif /* _AMD64_MACHINE_FRAMEASM_H */
