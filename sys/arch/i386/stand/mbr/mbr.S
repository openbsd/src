/*	$OpenBSD: mbr.S,v 1.1 1997/03/31 03:12:19 weingart Exp $	*/

/*
 * Copyright (c) 1997 Michael Shalayeff
 */

/* Copyright (c) 1996 VaX#n8 (vax@linkdead.paranoia.com)
 * last edited 9 July 1996
 * many thanks to Erich Boleyn (erich@uruk.org) for putting up with
 * all my questions, and for his work on GRUB
 * You may use this code or fragments thereof in a manner consistent
 * with the other copyrights as long as you retain my pseudonym and
 * this copyright notice in the file.
 */

/*	$OpenBSD: mbr.S,v 1.1 1997/03/31 03:12:19 weingart Exp $	*/

/*
 * Copyright (c) 1996 Michael Shalayeff
 */

/* Copyright (c) 1996 VaX#n8 (vax@linkdead.paranoia.com)
 * last edited 9 July 1996
 * many thanks to Erich Boleyn (erich@uruk.org) for putting up with
 * all my questions, and for his work on GRUB
 * You may use this code or fragments thereof in a manner consistent
 * with the other copyrights as long as you retain my pseudonym and
 * this copyright notice in the file.
 */

	.file	"start.S"

#include <machine/asm.h>
#include <machine/specialreg.h>

#define data32	.byte 0x66
#define addr32	.byte 0x67

#define BOOTBIOS	0x7c0	/* segment where we are loaded */
#define BOOTRELOC	0x7a0	/* segment where to relocate */
#define SIGNATURE	0xaa55	/* MBR signature */
#define NUMPART		4	/* number of partitions in partition table */
#define PARTSZ		16	/* each partition table entry is 16 bytes */
#define BSDPART		0xA6	/* OpenBSD partition */
#define OLDBSDPART	0xA5	/* {386,Net,Free}BSD partition */
#define BOOTABLE	0x80	/* bootable partition */

#ifdef DEBUG
#define CHAR_S		'S'	/* started */
#define CHAR_L		'L'	/* looking up bootable partition */
#define CHAR_P		'P'	/* partition table corrupted */
#define CHAR_O		'O'	/* force OpenBSD load */
#define CHAR_B		'B'	/* loading boot */
#define CHAR_G		'G'	/* jumping to boot */

#define DBGMSG(msg)		\
	movb    $msg, %al;	\
	data32;			\
	call    chr
#else /* !DEBUG */
#define DBGMSG(msg)
#endif /* !DEBUG */

	.text

	.globl	start
start:
	movl	%cs, %ax
	# set up stack(%ss:%esp)
	cli			/* disable interrupts w/o stack */
	movl	%ax, %ss
	data32
	movl	$0xfffc, %esp
	sti			/* we have stack, do ints */
	/* setup %ds */
	movl	%ax, %ds

	/* relocate the code to leave the space for boot stage */
	data32
	movl	$BOOTRELOC, %eax
	movl	%ax, %es
	xorl	%si, %si
	xorl	%di, %di
	xorl	%cx, %cx
	incb	%ch	/* movl $0x100, %ecx */
	cld
	rep
	movsw

	# jump to the relocated code
	data32
	ljmp $BOOTRELOC, $1f
1:
	DBGMSG(CHAR_S)

	/* setup %es, %ds */
	pushl	%ds
	popl	%es	/* next boot is at the same place as we were loaded */
	pushl	%cs
	popl	%ds	/* and %ds is at the %cs */

#ifdef SERIAL
	# Initialize the serial port to 9600 baud, 8N1.
	xorl	%ax, %ax
       	movb	$0xe3, %ax
	data32
	movl	$SERIAL, %dx
	int	$0x14
#endif

	/* bootstrap passes us drive number in %dl */
	testb	$0x80, %dl
	jnz	1f

	/* mbr on floppy ??? */
	data32
	movl	$fdmbr, %esi
	data32
	call	message

1:	xorl	%bx, %bx
	# cmpw	$SIGNATURE, (%bx)
	.byte	0x81, 0xbf
	.word	signature
	.word	SIGNATURE
	je	1f
	DBGMSG(CHAR_P)

	/* find the first active partition */
1:	DBGMSG(CHAR_L)
	data32
	movl	$pt, %esi
	data32
	movl	$NUMPART, %cx
1:
	# movb	(%si), %al
	.byte	0x8a, 0x44, 0x00
	cmpb	$BOOTABLE, %al
	je	2f
	data32
	addl	$PARTSZ, %esi
	loop	1b
	jmp	1f	/* no bootable -- find $BSDPART */

2:	# test if bootable is ours
	# movb	4(%si), %al	# partition type
	.byte   0x8a, 0x44, 0x04
	cmpb	$BSDPART, %al
	je	found

	/* else: ask! */
	data32
	movl	$prompt, %esi
	data32
	call	message

	# BIOS call: read char from kbd
	#	return: %ah == scancode, %al == ascii
	xorl	%ax, %ax
	int	$0x16

	/* load active, if not */
	cmpb	'n', %al
	je	found
	cmpb	'N', %al
	je	found

1:
	/* else: find OpenBSD partition */
	DBGMSG(CHAR_O)
	data32
	movl	$pt, %esi
	data32
	movl	$NUMPART, %ecx
1:
	# movb	(%si), %al
	.byte	0x8a, 0x44, 0x00
	cmpb	$BSDPART, %al
	je	found
	data32
	addl	$PARTSZ, %esi
	loop	1b
	data32
	movl	$enoboot, %esi
err_stop:
	data32
	call	message
	cli
	hlt

found:
	DBGMSG(CHAR_B)
	# movb	1(%si), %dh	# head
	.byte   0x8a, 0x74, 0x01
	# movw	2(%si), %cx	# sect, cyl
	.byte   0x8b, 0x4c, 0x02
	# movb	4(%si), %al	# partition type
	.byte   0x8a, 0x44, 0x04

/*
# BIOS call "INT 0x13 Function 0x2" to read sectors from disk into memory
#       Call with       %ah = 0x2
#                       %al = number of sectors
#                       %ch = cylinder
#                       %cl = sector
#                       %dh = head
#                       %dl = drive (0x80 for hard disk, 0x0 for floppy disk)
#                       %es:%bx = segment:offset of buffer
#       Return:
#                       %al = 0x0 on success; err code on failure
*/
	data32
	movl	$0x200 | 1, %eax	/* number of blocks */
	xorl	%bx, %bx		/* put it at %es:0 */
	int	$0x13
	data32
	movl    $eread, %si
	jc	err_stop

	DBGMSG(CHAR_G)

	# jump to the new code (%ds:%si is at he right point)
	data32
	ljmp	$0, $BOOTBIOS << 4
	/* not reached */

#
#	message: write the error message in %ds:%si to console
#
chr:
/*
#ifndef SERIAL
# BIOS call "INT 10H Function 0Eh" to write character to console
#	Call with	%ah = 0x0e
#			%al = character
#			%bh = page
#			%bl = foreground color
#else
# BIOS call "INT 14H Function 01h" to write character to console
#	Call with	%ah = 0x01
#			%al = character
#			%dx = port number
#endif
*/
	pushl	%eax

#ifndef SERIAL
	pushl	%ebx
	movb	$0x0e, %ah
	xorl	%bx, %bx
	incl	%bx		/* movw $0x01, %bx */
	int	$0x10
	popl	%ebx
#else
	pushl	%edx
	movb	$0x01, %ah
	data32
	movl	SERIAL, %dx
	int	$0x14
	popl	%edx
#endif
	popl	%ax
	data32
	ret

/*
 * Display string
 */
message:
	pushl	%ax
	cld
1:
	lodsb			# load a byte into %al
	testb	%al, %al
	jz	1f
	data32
	call	chr
	jmp	1b
1:
	popl	%ax
	ret

/* error messages */

fdmbr:	.asciz		"MBR on fd?\r\n"
eread:	.asciz		"Read error\r\n"
enoboot: .asciz		"No partition to boot\r\n"
prompt:	.asciz		"OpenBSD? "

endofcode:
	nop

/* throw in a partition table */
/* flag, head, sec, cyl, type, ehead, esect, ecyl, start, len */
	. = 0x1be	# starting address of partition table
pt:
	.byte	0x0,0,0,0,0,0,0,0
	.long	0,0
	.byte	0x0,0,0,0,0,0,0,0
	.long	0,0
	.byte	0x0,0,0,0,0,0,0,0
	.long	0,0
	.byte	BOOTABLE,0,1,0,BSDPART,255,255,255
	.long	0,50000
/* the last 2 bytes in the sector 0 contain the signature */
	. = 0x1fe
signature:
	.short	SIGNATURE
	. = 0x200
