/*	$OpenBSD: start.s,v 1.2 1996/04/26 18:23:12 mickey Exp $	*/
/*	$NetBSD: start.s,v 1.4 1994/10/27 04:21:25 cgd Exp $	*/

#include "asm.h"

/* At entry, the processor is in 16 bit real mode and the code is being
 * executed from an address it was not linked to. Code must be pic and
 * 32 bit sensitive until things are fixed up.
 */

	.word	0xaa55			/* bios extension signature */
	.byte	0			/* no. of 512B blocks */
	jmp	1f			/* enter from bios here */
	.byte	0			/* checksum */
ENTRY(start)
1:
	cli
	/* save the bios return address in these registers until protected
	 * mode %ds is set up
	 *	mov	(%esp), %edx
	 *	mov	2(%esp), %ebp
	 */
	pop	%dx			/* return offset */
	pop	%ebp			/* return segment */

	/*  save stack [, data] context in case we are under dos */
	mov	%esp, %ecx
	mov	%ss, %ax
	mov	%eax, %ebx

	/* set up a usable stack */
	.byte	0xb8			/* (mov $0xa000, %ax) */
	.word	0xa000
	mov	%ax, %ss
	xor	%esp, %esp

	push	%ebp			/* return segment */
	push	%dx			/* return offset */
	push	%ds

#if notdef
	jmp	ret16
#endif


#ifdef CHECK_386
	/*
	 * check if 386 or later
	 * from Intel i486 programmer's reference manual, section 22.10
	 * Care must be taken with the first few instructions to ensure
	 * operations are compatible with 386 progenitors - no operand
	 * or address size overrides, all operations must be 16 bit.
	 * Real mode not well supported by gas so it looks a bit crufty
	 *
	 * TBD - there is little stack space, although the routine below
	 * uses only two bytes; might set up new stack first thing, then
	 * check processor type - would mean carrying sp, ss in two gp
	 * registers for a while. also make alternate provisions for saving
	 * ds: below.
	 */
	pushf
	pop	%ebx
	.byte	0x81, 0xe3		/* (and 0x0fff, %ebx) */
	.word	0x0fff
	push	%ebx
	popf
	pushf
	pop	%eax
	.byte	0x25 			/* (and 0xf000, %eax) */
	.word	0xf000
	.byte	0x3d			/* (cmp 0xf000, %eax) */
	.word	0xf000
	jz	Lwrong_cpu		/* \'86 */
	.byte	0x81, 0xcb		/* (or 0xf000, %ebx) */
	.word	0xf000
	push	%ebx
	popf
	pushf
	pop	%eax
	.byte	0x25			/* (and 0xf000, %eax) */
	.word	0xf000
	jnz	Lcpu_ok			/* else is \'286 */

Lwrong_cpu:
	.byte	0xbb			/* (mov bad_cpu_msg, %ebx) */
	.word	bad_cpu_msg
	.byte	0xe8			/* (call xputs) */
	.word	xputs-.-2
	lret

xputc:	/* print byte in %al */
	data32
	pusha
	.byte	0xbb			/* (mov $0x1, %ebx) %bh=0, %bl=1 (blue) */
	.word	0x0001
	movb	$0xe, %ah
	/* sti */
	int	$0x10			/* display a byte */
	/* cli */
	data32
	popa
	ret

xputs:	/* print string pointed to by cs:bx */
	data32
	pusha
1:
	cs
	.byte	0x8a, 0x07		/* (mov (%ebx), %al) */
	cmpb	$0, %al
	jz	1f
	push	%ebx
	.byte	0xe8			/* (call xputc) */
	.word	xputc-.-2
	pop	%ebx
	inc	%ebx
	jmp	1b
1:
	data32
	popa
	ret

bad_cpu_msg:	.asciz	"netboot: cpu cannot execute '386 instructions, net boot not done.\n\r"

Lcpu_ok:
	/*
	 * at this point it is known this can execute 386 instructions
	 * so operand and address size prefixes are ok
	 */
#endif /* CHECK_386 */

	/* copy rom to link addr, prepare for relocation */
        xor     %esi, %esi		/* source */
        opsize
        mov     $0, %edi 		/* destination */
        opsize
        mov     $(RELOC)>>4, %eax
        mov     %ax, %es
        opsize
	mov     $(ROM_SIZE), %ecx		/* count */
        cs
        rep
        movsb

	addrsize
	cs
	lgdt	gdtarg-RELOC

	/* turn on protected mode */
	cli
	mov	%cr0, %eax
	opsize
	or	$CR0_PE, %eax
	mov	%eax, %cr0

	/* jump to relocation, flush prefetch queue, and reload %cs */
	opsize
	ljmp	$KERN_CODE_SEG, $1f
1:
	/* reload other segment registers */
	movl	$KERN_DATA_SEG, %eax
	movl	%ax, %ds
	movl	%ax, %es
	movl	%ax, %ss
	movl	$0xa0000, %esp
	call	_main
	call	_exit

_ExitToBios:
	.globl _ExitToBios
	/* set up a dummy stack frame for the second seg change. */
	mov	$(RELOC)>>4, %eax
	pushw	%ax			/* real cs */
	pushw	$2f			/* real pc */

	/* Change to use16 mode. */
	ljmp	$BOOT_16_SEG, $1f	/* jump to a 16 bit segment */
1:
	/* clear the PE bit of CR0 */
	mov	%cr0, %eax
	opsize
	and 	$0!CR0_PE, %eax
	mov	%eax, %cr0

	/* make intersegment jmp to flush the processor pipeline
	 * using the fake stack frame set up earlier
	 * and reload CS register
	 */
	lret
2:
	/* we are in real mode now
	 * set up the real mode segment registers : DS, SS, ES
	 */
	movw	%cs, %ax
	movw	%ax, %ds
	movw	%ax, %ss
	movw	%ax, %es

ret16:	/* temporary label - remove (TBD) */
	/* now in dumbed down mode, caveats */
	/* restore old context and return to whatever called us */
	pop	%ds
	pop	%dx
	pop	%ebp

	mov	%ebx, %eax
	mov	%ax, %ss
	mov	%ecx, %esp

	push	%ebp
	push	%dx	
	sti
	lret

#ifdef USE_BIOS
_real_to_prot:
	.global	_real_to_prot

	addrsize
	cs
	lgdt	gdtarg-RELOC
	cli
	mov	%cr0, %eax
	opsize
	or	$CR0_PE, %eax
	mov	%eax, %cr0

	/* jump to relocation, flush prefetch queue, and reload %cs */
	opsize
	ljmp	$KERN_CODE_SEG, $1f
1:
	movl	$KERN_DATA_SEG, %eax
	movl	%ax, %ds
	movl	%ax, %es
	movl	%ax, %ss

	ret
#endif

#ifdef USE_BIOS
_prot_to_real:
	.global	_prot_to_real

	/* set up a dummy stack frame for the second seg change. */
	movl 	$(RELOC), %eax
	sarl	$4, %eax
	pushw	%ax			/* real cs */
	pushw	$2f			/* real pc */

	/* Change to use16 mode. */
	ljmp	$BOOT_16_SEG, $1f	/* jump to a 16 bit segment */
1:
	/* clear the PE bit of CR0 */
	mov	%cr0, %eax
	opsize
	and 	$0!CR0_PE, %eax
	mov	%eax, %cr0

	/* make intersegment jmp to flush the processor pipeline
	 * using the fake stack frame set up earlier
	 * and reload CS register
	 */
	lret
2:
	/* we are in real mode now
	 * set up the real mode segment registers : DS, SS, ES
	 */
	movw	%cs, %ax
	movw	%ax, %ds
	movw	%ax, %ss
	movw	%ax, %es

	opsize
	ret
#endif

	.align	4
gdt:
	.word	0, 0
	.byte	0, 0x00, 0x00, 0

	/* code segment */
	.word	0xffff, 0
	.byte	0, 0x9f, 0xcf, 0

	/* data segment */
	.word	0xffff, 0
	.byte	0, 0x93, 0xcf, 0

	/* 16 bit real mode */
	.word	0xffff, 0
	.byte	0, 0x9e, 0x00, 0

	.align	4
gdtarg:
	.word	0x1f			/* limit */
	.long	gdt			/* addr */
