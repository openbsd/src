/*	$OpenBSD: biosboot.S,v 1.2 1997/03/31 03:12:01 weingart Exp $	*/

	.file	"bootbios.S"

#include <machine/asm.h>
#include <machine/specialreg.h>
#define _LOCORE
#include <machine/segments.h>
#undef _LOCORE

#define addr32  .byte 0x67
#define data32  .byte 0x66

#ifdef	DEBUG
#define	BLKCNT	32
#else
#define	BLKCNT	48
#endif

#define BIOSSEG		0x07c0	/* boot loaded here */
#define BOOTSEG		0x1000	/* /boot placement */
#define BOOTSTACK	0xfffc	/* /boot stack */
#define ZMAGIC		0x0b01	/* ZMAGIC */

#ifdef DEBUG
#define DBGMSG(msg)		\
	movb    $msg, %al;	\
	data32;			\
	call    chr
#define CHAR_M	'M'	/* magic check */
#define CHAR_P	'P'	/* switch to protected mode */
#else /* !DEBUG */
#define DBGMSG(msg)
#endif /* !DEBUG */

	.text

	.globl	start
start:
	/* adjust %cs */
	data32
	ljmp $BIOSSEG, $2f
1:	.asciz	"loading /boot"

2:	/* set up stack (%ss:%esp) */
	cli			/* disable interrupts w/o stack */
	data32
	movl	$BOOTSEG, %ax
	movl	%ax, %ss
	data32
	movl	$BOOTSTACK, %esp
	sti			/* we have stack, do ints */

	/* set up %es, (where we will load /boot to) */
	movl	%ax, %es

	/* set up %ds */
	pushl	%cs
	popl	%ds

#ifdef SERIAL
	# Initialize the serial port to 9600 baud, 8N1.
	pushl	%dx
	movl	$0x00e3, %ax
	data32
	movl	SERIAL, %edx
	int	$0x14
	popl	%dx
#endif

	data32
	movl	$1b, %esi
	data32
	call	message

	data32
	xorl	%ebx, %ebx		/* put it at %es:0 */
	addr32
	movb	_block_count, %cl	/* how many to read */
	movzbl	%cl, %ecx
	data32
	movl	$_block_table, %esi

1:
	pushl	%ecx
	cld
	lodsl	/* word */	/* cylinder/sector */
	movl	%ax, %cx
	lodsb			/* head */
	movb	%al, %dh
	lodsb			/* # of sectors to load */
	movb	$0x2, %ah
	pushl	%ax
	int	$0x13
	jnc	3f

	/* read error */
       	data32
	movl    $2f, %esi
	jmp	halt
2:	.asciz	"\r\nRead error\r\n"

3:	/* read next block */
	movb	$'.', %al
	data32
	call	chr		/* show progress indicator */

	popl	%ax
	data32
	movzbl	%al, %eax
	shll	$9, %ax		/* 512 bytes sectors */
	addl	%ax, %bx
	popl	%ecx
	loop	1b

	data32
	movl    $2f, %esi	/* new line */
	data32
	call	message

#if 0
	DBGMSG(CHAR_M)
#endif

	xorl	%esi, %esi
	cld
	/* check /boot magic */
	es;lodsl;es;lodsl	/* no need for high word */
	# cmpw	$ZMAGIC, %ax
	.byte	0x3d
	.word	ZMAGIC
	je	3f

	data32
	movl	$1f, %esi
halt:
	data32
	call	message
	cli
	hlt
1:	.ascii	"Bad magic"
2:	.asciz	"\r\n"
3:
#if notdef
	data32;es;lodsl	/* text size */
	data32;es;lodsl	/* data size */
	data32;es;lodsl	/* bss  size */
	data32;es;lodsl	/* syms size */
#endif
#if 0
	data32;addr32
	addl	16(%esi), %edi	/* entry */
	data32
	subl	$REL, %edi
#endif

#if 0
	DBGMSG(CHAR_P)
#endif

	/* change to protected mode */
	/* guarantee that interrupts are disabled when in prot mode */
	cli

	/* load the gdtr */
	addr32
	data32
	lgdt	Gdtr

	/* set the PE bit of CR0 */
	movl	%cr0, %eax
	data32
	orl	$CR0_PE, %eax
	movl	%eax, %cr0 

	/*
	 * make intrasegment jump to flush the processor pipeline and
	 * reload CS register
	 */
	data32
	ljmp	$8, $1f+(BIOSSEG << 4)

1:	/*
	 * 32bit mode
	 * set up %ds, %ss, %es
	 */
	movl	$0x10, %eax
	movl	%ax, %ds
	movl	%ax, %ss
	movl	%ax, %es
	movl	%ax, %fs
	movl	%ax, %gs
	movl	$BOOTSTACK, %esp

#if 0
#ifdef	DEBUG
	movl	$0xb8000, %ebx
	movl	$0x074f0747, (%ebx)
#endif
#endif

	movzbl	%dl, %eax	/* drive number is in the lowest byte */
	pushl	%eax

#if 0
#ifdef DEBUG
	movl	$0x10000, %ebx
	movl	(%ebx), %ecx
	/* cmpw $ZMAGIC, %ax */
	.byte	0x3d
	.word	ZMAGIC
je	1f
	movl	$0xb8004, %ebx
	movl	$0x07500748, (%ebx)
	jne	2f
1:
	movl	$0xb8004, %ebx
	movl	$0x074f0747, (%ebx)
2:
#endif
#endif

	/* jmp	/boot */
	ljmp	$0x8, $(REL << 4) + 0x20
	/* not reached */

#
#	chr: write the character in %al to console
#
chr:
	data32
	pushl	%eax

#ifndef SERIAL
	data32
	pushl	%ebx
	movb	$0x0e, %ah
	xorl	%bx, %bx
	incl	%bx		/* movw $0x01, %bx */
	int	$0x10
	data32
	popl	%ebx
#else
	data32
	pushl	%edx
	movb	$0x01, %ah
	xorl	%dx, %dx
	movb	SERIAL, %dl
	int	$0x14
	data32
	popl	%edx
#endif
	data32
	popl	%eax
	data32
	ret

/*
 * Display string
 */
message:
	data32
	pushl	%eax
	cld
1:
	lodsb			# load a byte into %al
	testb	%al, %al
	jz	1f
	data32
	call	chr
	jmp	1b
1:
	data32
	popl	%eax
	data32
	ret

	.align	2
Gdtr:	.word	3 * 8 - 1
	.long	(BIOSSEG << 4) + 2f
	.align	3
2:		/* 0x00 : null */
	.long	0, 0
		/* 0x08 : flat code */
	.word	0xFFFF			# lolimit
	.word	0			# lobase
	.byte	0			# midbase
	.byte	SDT_MEMERAC | 0 | 0x80	# RWXAC, dpl = 0, present
	.byte	0xf | 0 | 0x40 | 0x80	# hilimit, xx, 32bit, 4k granularity
	.byte	0			# hibase
		/* 0x10 : flat data */
	.word	0xFFFF			# lolimit
	.word	0			# lobase
	.byte	0			# midbase
	.byte	SDT_MEMRWA | 0 | 0x80	# RWA, dpl = 0, present
	.byte	0xf | 0 | 0x40 | 0x80	# hilimit, xx, 32bit, 4k granularity
	.byte	0			# hibase

	.globl	_block_table
_block_table:
	.word	0	/* cyllinder/sector */
	.byte	0	/* head */
	.byte	0	/* nsect */
	. = _block_table + BLKCNT*4
	.globl	_block_count
_block_count:
	.byte	BLKCNT	/* entries in _block_table */

	. = 0x200 - 4
	/* a little signature */
	.ascii	"boot"

