/*	$OpenBSD: biosboot.S,v 1.13 1997/07/31 20:04:40 mickey Exp $	*/

	.file	"bootbios.S"

#include <machine/asm.h>
#include <assym.h>

#define addr32  .byte 0x67
#define data32  .byte 0x66

#ifdef	DEBUG
#define	BLKCNT	12	/* Max without colliding with the partition table */
#else
#define	BLKCNT	16
#endif

#define BOOTSEG		0x07c0	/* boot loaded here */
#define BOOTSTACK	0xfffc	/* stack starts here */
#define ZMAGIC		0x0b01	/* ZMAGIC */

/*
 * Partition table values
 */
#define BOOTABLE	0x80	/* flag of boot_ind, means bootable partition */
#define OPENBSDPART	0xA6	/* OpenBSD partition type */

#ifdef DEBUG
#define DBGMSG(msg)		\
	movb    $msg, %al;	\
	.byte	0xe8;		\
	.word	Lchr - . - 2
#define GO_RELOC	'R'	/* running relocated code */
#define REAL2PROT	'P'	/* switch to protected mode */
#else /* !DEBUG */
#define DBGMSG(msg)	/* */
#endif /* !DEBUG */

	.text

	.globl	start
start:
	jmp	1f

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
	.asciz	"NO LABEL  "
	.asciz	"UFS 4.4"

	/* boot code */
	. = start + 0x3e

1:	/* set up stack (%ss:%esp) */
	cli			/* disable interrupts w/o stack */
	# movw	$BOOTREL, %ax
	.byte	0xb8
	.word	BOOTREL
	movl	%ax, %ss
	movl	%ax, %es	/* relocate there */
	data32
	movl	$BOOTSTACK, %esp
	sti			/* we have stack, do ints */

	# movw	$BOOTSEG, %ax	/* we are here */
	.byte	0xb8
	.word	BOOTSEG
	movl	%ax, %ds
	xorl	%si, %si
	xorl	%di, %di
	# movw	$0x100, %cx	/* 512 bytes to move */
	.byte	0xb9
	.word	0x100
	cld
	rep;	movsl

	/* jump to relocated code */
	data32
	ljmp $BOOTREL, $2f
1:	.asciz	"loading /boot"

2:	DBGMSG(GO_RELOC)

	nop
	/* set up %ds */
	pushl	%cs
	popl	%ds

	/* set up %es, (where we will load /boot to) */
	# movw	$(START >> 4), %ax
	.byte	0xb8
	.word	START >> 4
	movl	%ax, %es

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

	data32
	movl	$1b, %esi
	# call	Lmessage
	.byte	0xe8
	.word	Lmessage - . - 2

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
	jmp	5f
2:	.asciz	"\r\nRead error\r\n"

3:	/* read next block */
	movb	$'.', %al
	# call	Lchr		/* show progress indicator */
	.byte	0xe8
	.word	Lchr - . - 2

	popl	%ax
	movzbl	%al, %ax
	shll	$9, %ax		/* 512 bytes sectors */
	addl	%ax, %bx
	popl	%cx
	loop	1b

	data32
	movl    $2f, %esi	/* new line */
	# call	Lmessage
	.byte	0xe8
	.word	Lmessage - . - 2

	xorl	%si, %si
	cld
	/* check /boot magic */
	es;lodsl;es;lodsl	/* no need for high word */
	# cmpw	$ZMAGIC, %ax
	.byte	0x3d
	.word	ZMAGIC
	je	3f

	data32
	movl	$1f, %esi
5:
	# call	Lmessage
	.byte	0xe8
	.word	Lmessage - . - 2
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
	data32
	addl	16(%esi), %edi	/* entry */

	DBGMSG(REAL2PROT)

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
	ljmp	$8, $1f+(BOOTREL << 4)

1:	/*
	 * 32bit mode
	 * set up %ds, %ss, %es
	 */
	movw	$0x10, %eax
	movl	%ax, %ds
	movl	%ax, %ss
	movl	$BOOTSTACK, %esp

#ifdef	DEBUG
	movl	$0xb8004, %ebx
	movl	$0x074f0747, (%ebx)
#endif

	movzbl	%dl, %eax	/* drive number is in the lowest byte */
	pushl	%eax
	pushl	$BOOTMAGIC	/* use some magic */

	/* jmp	/boot */
	ljmp	$8, $(START + 0x20)
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
	.long	(BOOTREL << 4) + 1b

	.globl	_block_table
_block_table:
	.word	0	/* cyllinder/sector */
	.byte	0	/* head */
	.byte	0	/* nsect */
	. = _block_table + BLKCNT*4
	.globl	_block_count
_block_count:
	.byte	BLKCNT	/* entries in _block_table */

	. = 0x1b8
	.space	4, 0	/* (MBR) NT registry offset from James C. Cortilier III */
	. = 446
	.globl	_partitions
	/* throw in a partition in case we are block0 as well */
	/* flag, head, sec, cyl, typ, ehead, esect, ecyl, start, len */
_partitions:
	.byte   BOOTABLE, 0, 1, 0,OPENBSDPART, 255, 255, 255
	.long   0,50000
	.byte   0,0,0,0,0,0,0,0
	.long   0,0
	.byte   0,0,0,0,0,0,0,0
	.long   0,0
	.byte   0,0,0,0,0,0,0,0
	.long   0,0
	
	. = 0x200 - 2
	/* a little signature */
	.word	0xaa55
