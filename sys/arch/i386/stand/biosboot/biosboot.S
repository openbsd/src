/*	$OpenBSD: biosboot.S,v 1.35 2003/06/27 18:27:53 weingart Exp $	*/

/*
 * Copyright (c) 2003 Tobias Weingartner
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
	.file	"biosboot.S"

#include <machine/asm.h>
#include <assym.h>

#define	BLKCNT	63

#define BOOTSEG		0x07c0	/* biosboot loaded here */
#define BOOTSTACK	0xfffc	/* stack starts here */
#define LFMAGIC		0x464c  /* LFMAGIC (only uses two bytes of \7fELF */


/* Clobbers %al - maybe more */
#define putc(c)			\
	movb	$c, %al;	\
	call	Lchr;

/* Clobbers %esi - maybe more */
#define	puts(s)			\
	mov	$s, %si;	\
	call	Lmessage


	.text
	.code16
	.globl	_start
_start:
	jmp	1f
	nop

	. = _start + 3
	.asciz	"OpenBSD"
	/* BPB */
	. = _start + 0x0b
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
	. = _start + 0x1c
ebpb:	.long	16		/* hidden sectors */
	.long	0		/* large sectors */
	.word	0		/* physical disk */
	.byte	0x29		/* signature, needed by NT */
	.space	4, 0		/* volume serial number */
	.asciz	"UNIX LABEL"
	.asciz	"UFS 4.4"

	/* boot code */
	. = _start + 0x3e

1:
	/* Fix up %cs just in case */
	ljmp	$BOOTSEG, $1f

load_msg:
	.asciz	"reading boot"

1:
	/* set up stack (%ss:%sp) */
	cli			/* disable interrupts w/o stack */
	xor	%ax, %ax
	mov	%ax, %ss
	mov	$BOOTSTACK, %sp
	sti			/* we have stack, do ints */

	/* Set up needed data segment reg */
	mov	$BOOTSEG, %ax
	mov	%ax, %ds

#ifdef SERIAL
	# Initialize the serial port to 9600 baud, 8N1.
	push	%dx
	mov	$0x00e3, %ax
	mov	SERIAL, %dx
	int	$0x14
	pop	%dx
#endif

#ifdef BDEBUG
	putc('R')
#endif

	/* Print pretty message */
	puts(load_msg)

	/* set up %es, (where we will load /boot to) */
	mov	$(LOADADDR >> 4), %ax
	mov	%ax, %es

	xorw	%bx, %bx		/* put it at %es:0 */
	movb	block_count, %cl	/* how many to read */
	movzbw	%cl, %cx
	movw	$block_table, %si

1:
	push	%cx
	putc('.')		/* show progress indicator */
	cld
	lodsw	/* word */	/* cylinder/sector */
	mov	%ax, %cx
	lodsb			/* head */
	movb	%al, %dh
	lodsb			/* # of sectors to load */
	movb	$0x2, %ah
	push	%ax
	int	$0x13
	jnc	3f

	/* read error */
	puts(2f)
	jmp	halt
2:	.asciz	"\r\nRead error\r\n"

3:	/* read next block */

	pop	%ax
	movzbw	%al, %ax
	shl	$9, %ax		/* 512 bytes sectors */
	add	%ax, %bx
	pop	%cx
	loop	1b

	puts(2f)

	xor	%si, %si
	cld
	/* check /boot magic */
	es;lodsw;es;lodsw	/* no need for high word */
	cmp	$LFMAGIC, %ax
	je	3f

	puts(1f)
halt:
	cli
99:
	hlt
	jmp 99b
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

	movzbl	%dl, %eax	/* drive number is in the lowest byte */
	pushl	%eax
	pushl	$BOOTMAGIC	/* use some magic */

	/* jmp	/boot */
	ljmp $(LINKADDR >> 4), $0
	/* not reached */

/*
 * Display string
 */
Lmessage:
	push	%ax
	cld
1:
	lodsb			# load a byte into %al
	testb	%al, %al
	jz	1f
	call	Lchr
	jmp	1b

#
#	Lchr: write the character in %al to console
#
Lchr:
	push	%ax

#ifndef SERIAL
	push	%bx
	movb	$0x0e, %ah
	xor	%bx, %bx
	inc	%bx		/* movw $0x01, %bx */
	int	$0x10
	pop	%bx
#else
	push	%dx
	movb	$0x01, %ah
	xor	%dx, %dx
	movb	SERIAL, %dl
	int	$0x14
	pop	%dx
#endif
1:
	pop	%ax
	ret

	#.data
	.globl	block_table, block_count
	.type block_count, @function
	.type block_table, @function
block_count:
	.byte	BLKCNT	/* entries in block_table */
block_table:
	.word	0	/* cylinder/sector */
	.byte	0	/* head */
	.byte	0	/* nsect */
	. = block_table + BLKCNT*4

	. = 0x200 - 2
	/* a little signature */
	.word	DOSMBR_SIGNATURE
