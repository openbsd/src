/*	$OpenBSD: biosboot.S,v 1.28 1998/07/04 08:56:41 deraadt Exp $	*/

/*
 * Copyright (c) 1997 Michael Shalayeff, Tobias Weingartner
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
	.file	"bootbios.S"

#include <machine/asm.h>
#include <assym.h>

#define addr32  .byte 0x67
#define data32  .byte 0x66

#define	BLKCNT	12	/* Max without colliding with the partition table */

#define BOOTSEG		0x07c0	/* boot loaded here */
#define BOOTSTACK	0xfffc	/* stack starts here */
#define ZMAGIC		0x0b01	/* ZMAGIC */


/* Clobbers %al - maybe more */
#define putc(c)			\
	movb	$c, %al;	\
	.byte	0xe8;		\
	.word	Lchr - . - 2

/* Clobbers %esi - maybe more */
#define	puts(s)			\
	data32;			\
	movl	$s, %esi;	\
	.byte	0xe8;		\
	.word	Lmessage - . - 2


	.text
	.globl	start
start:
	jmp	1f
	nop

	. = start + 3
	.asciz	"OpenBSD"
	/* BPB */
	. = start + 0x0b
bpb:	.word	DEV_BSIZE	/* sector size */
	.byte	1		/* sectors/cluster */
	.word	0		/* reserved sectors */
	.byte	0		/* # of FAT */
	.word	0		/* root entries */
	.word	0		/* small sectors */
	.byte	0xf8		/* media type (hd) */
	.word	0		/* sectors/fat */
	.word	0		/* sectors per track */
	.word	0		/* # of heads */

	/* EBPB */
	. = start + 0x1c
ebpb:	.long	16		/* hidden sectors */
	.long	0		/* large sectors */
	.word	0		/* physical disk */
	.byte	0x29		/* signature, needed by NT */
	.space	4, 0		/* volume serial number */
	.asciz	"UNIX LABEL"
	.asciz	"UFS 4.4"

	/* boot code */
	. = start + 0x3e

1:
	/* Fix up %cs just in case */
	data32
	ljmp	$BOOTSEG, $1f

load_msg:
	.asciz	"reading boot"

1:
	/* set up stack (%ss:%esp) */
	cli			/* disable interrupts w/o stack */
	xorl	%ax, %ax
	movl	%ax, %ss
	data32
	movl	$BOOTSTACK, %esp
	sti			/* we have stack, do ints */

	/* Set up other segment regs */
	# movw	$BOOTSEG, %ax
	.byte	0xb8
	.word	BOOTSEG
	movl	%ax, %ds
	movl	%ax, %es
	movl	%ax, %fs
	movl	%ax, %gs

#ifdef SERIAL
	# Initialize the serial port to 9600 baud, 8N1.
	pushl	%dx
	# movw	$0x00e3, %ax
	.byte	0xb8
	.word	0x00e3
	# movw	SERIAL, %dx
	.byte	0xba
	.word	SERIAL
	int	$0x14
	popl	%dx
#endif

#ifdef BDEBUG
	putc('R')
#endif

	/* Print pretty message */
	puts(load_msg)

	/* set up %es, (where we will load /boot to) */
	# movw	$(LOADADDR >> 4), %ax
	.byte	0xb8
	.word	LOADADDR >> 4
	movl	%ax, %es

	data32
	xorl	%ebx, %ebx		/* put it at %es:0 */
	addr32
	movb	_block_count, %cl	/* how many to read */
	movzbl	%cl, %ecx
	# movw	$_block_table, %si
	.byte	0xbe
	.word	_block_table

1:
	pushl	%cx
	putc('.')		/* show progress indicator */
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
	puts(2f)
	jmp	halt
2:	.asciz	"\r\nRead error\r\n"

3:	/* read next block */

	popl	%ax
	data32
	movzbl	%al, %eax
	shll	$9, %ax		/* 512 bytes sectors */
	addl	%ax, %bx
	popl	%cx
	loop	1b

	puts(2f)

	xorl	%si, %si
	cld
	/* check /boot magic */
	es;lodsl;es;lodsl	/* no need for high word */
	# cmpw	$ZMAGIC, %ax
	.byte	0x3d
	.word	ZMAGIC
	je	3f

	puts(1f)
halt:
	cli
	hlt
1:	.ascii	"Bad magic"
2:	.asciz	"\r\n"

3:	/* At this point we could try to use the entry point in
	 * the image we just loaded.  But if we do that, we also
	 * have to potentially support loading that image where it
	 * is supposed to go.  Screw it, just assume that the image
	 * is sane.
	 */
#ifdef BDEBUG
	putc('P')
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
	ljmp	$8, $(BOOTSEG << 4) + 1f

1:	/*
	 * 32bit mode
	 * set up %ds, %ss, %es, etc
	 */
	movl	$0x10, %eax
	movl	%ax, %ds
	movl	%ax, %ss
	movl	%ax, %es
	movl	%ax, %fs
	movl	%ax, %gs
	movl	$BOOTSTACK, %esp

#ifdef	BDEBUG
	movl	$0xb8004, %ebx
	movl	$0x074f0747, (%ebx)
#endif

	movzbl	%dl, %eax	/* drive number is in the lowest byte */
	pushl	%eax
	pushl	$BOOTMAGIC	/* use some magic */

	/* jmp	/boot */
	ljmp	$8, $(LOADADDR + 0x20)
	/* not reached */

/*
 * Display string
 */
Lmessage:
	data32
	pushl	%eax
	cld
1:
	lodsb			# load a byte into %al
	testb	%al, %al
	jz	1f
	# call	Lchr
	.byte	0xe8
	.word	Lchr - . - 2
	jmp	1b

#
#	Lchr: write the character in %al to console
#
Lchr:
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
1:
	data32
	popl	%eax
	ret

	.align	3
1:		/* 0x00 : null */
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
Gdtr:	.word	. - 1b
	.long	(BOOTSEG << 4) + 1b

	.globl	_block_table, _block_count
_block_count:
	.byte	BLKCNT	/* entries in _block_table */
_block_table:
	.word	0	/* cylinder/sector */
	.byte	0	/* head */
	.byte	0	/* nsect */
	. = _block_table + BLKCNT*4

#ifdef BEMBR
	. = 0x1b8
	.space	4, 0	/* NT registry offset from James C. Cortilier III */

 	. = DOSPARTOFF
 	.globl	_partitions
 	/* throw in a partition in case we are block0 as well */
	. = DOSPARTOFF
	.globl	_partitions
	/* throw in a partition in case we are block0 as well */
	/* flag, head, sec, cyl, typ, ehead, esect, ecyl, start, len */
_partitions:
	.byte   DOSACTIVE, 0, 1, 0, DOSPTYP_OPENBSD, 255, 255, 255
	.long   0,50000
	.byte   0,0,0,0,0,0,0,0
	.long   0,0
	.byte   0,0,0,0,0,0,0,0
	.long   0,0
	.byte   0,0,0,0,0,0,0,0
	.long   0,0
#endif
	. = 0x200 - 2
	/* a little signature */
	.word	DOSMBR_SIGNATURE
