/*	$NetBSD: start.S,v 1.12 1995/01/18 17:34:18 mycroft Exp $	*/

/*
 * Ported to boot 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*
  Copyright 1988, 1989, 1990, 1991, 1992 
   by Intel Corporation, Santa Clara, California.

                All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <machine/asm.h>
#define	addr32	.byte 0x67
#define	data32	.byte 0x66

BOOTSEG		=	0x0100	# boot will be loaded here (below 640K)
BOOTSTACK	=	0xfffc	# boot stack
SIGNATURE	=	0xaa55
LOADSZ		=	15	# size of unix boot
PARTSTART	=	0x1be	# starting address of partition table
NUMPART		=	4	# number of partitions in partition table
PARTSZ		=	16	# each partition table entry is 16 bytes
BSDPART		=	0xA5	# value of boot_ind, means bootable partition
BOOTABLE	=	0x80	# value of boot_ind, means bootable partition

	.text	
	.globl	start

ENTRY(boot1)
start:
	# start (aka boot1) is loaded at 0x0:0x7c00 but we want 0x7c0:0
	# ljmp to the next instruction to adjust %cs
	data32
	ljmp $0x7c0, $start1

start1:
	# set up %ds
	movl	%cs, %ax
	movl	%ax, %ds

	# set up %ss and %esp
	data32
	movl	$BOOTSEG, %eax
	movl	%ax, %ss
	data32
	movl	$BOOTSTACK, %esp

	/*** set up %es, (where we will load boot2 to) ***/
	movl	%ax, %es

#ifdef SERIAL
	# Initialize the serial port to 9600 baud, 8N1.
	data32
	pushl	%edx
	data32
	movl	$0x00e3, %eax
	xorl	%edx, %edx
	int	$0x14
	data32
	popl	%edx
#endif

#ifdef	DEBUG
	data32
	movl	$one, %esi
	data32
	call	message
#endif

	# bootstrap passes us drive number in %dl
	cmpb	$0x80, %dl
	data32
	jae	hd

fd:
#	reset the disk system
#ifdef	DEBUG
	data32
	movl	$two, %esi
	data32
	call	message
#endif
	movb	$0x00, %ah
	int	$0x13
#ifdef	DEBUG
	data32
	movl	$three, %esi
	data32
	call	message
#endif
	data32
	movl	$0x0001, %ecx	# cyl 0, sector 1
	movb	$0x00, %dh	# head
	# XXX Override the drive number.
	movb	$0x00, %dl
	data32
	jmp	load

hd:	/**** load sector 0 into the BOOTSEG ****/
#ifdef	DEBUG
	data32
	movl	$four, %esi
	data32
	call	message
#endif
	data32
	movl	$0x0201, %eax
	xorl	%ebx, %ebx	# %bx = 0
	data32
	movl	$0x0001, %ecx
#ifdef	DEBUG
	data32
	movl	$five, %esi
	data32
	call	message
#endif
	data32
	andl	$0xff, %edx
	/*mov	$0x0080, %edx*/
	int	$0x13
	data32
	jb	read_error

	/***# find the first 386BSD partition *****/
	data32
	movl	$PARTSTART, %ebx
	data32
	movl	$NUMPART, %ecx
again:
	addr32
	movb    %es:4(%ebx), %al
	cmpb	$BSDPART, %al
	data32
	je	found
	data32
	addl	$PARTSZ, %ebx
	data32
	loop	again
	data32
	movl	$enoboot, %esi
	data32
	jmp	err_stop


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

found:
	addr32
	movb	%es:1(%ebx), %dh /* head */
	addr32
	movl	%es:2(%ebx), %ecx /*sect, cyl (+ 2 bytes junk in top word) */

load:
	movb	$0x2, %ah	/* function 2 */
	movb	$LOADSZ, %al	/* number of blocks */
	xorl	%ebx, %ebx	/* %bx = 0, put it at 0 in the BOOTSEG */
	int	$0x13
	data32
	jb	read_error

	# ljmp to the second stage boot loader (boot2).
	# After ljmp, %cs is BOOTSEG and boot1 (512 bytes) will be used
	# as an internal buffer "intbuf".

#ifdef	DEBUG
	data32
	movl	$six, %esi
	data32
	call	message
#endif

	data32
	ljmp	$BOOTSEG, $_C_LABEL(boot2)

#
#	read_error
#

read_error:
	data32
	movl	$eread, %esi
err_stop:
	data32
	call	message
	data32
	jmp	stop

#
#	message: write the error message in %ds:%esi to console
#

message:
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
	data32
	pushl	%eax
	data32
	pushl	%ebx
	data32
	pushl	%edx

nextb:
	cld
	lodsb			# load a byte into %al
	testb	%al, %al
	data32
	jz	done

#ifndef SERIAL
	movb	$0x0e, %ah
	data32
	movl	$0x0001, %ebx
	int	$0x10
#else
	movb	$0x01, %ah
	xorl	%edx, %edx
	int	$0x14
#endif

	data32
	jmp	nextb

done:
	data32
	popl	%edx
	data32
	popl	%ebx
	data32
	popl	%eax
	data32
	ret


/* Conventional GDT indexes. */
BOOT_CS_INDEX	=	3
BOOT_CS16_INDEX	=	5
BOOT_DS_INDEX	=	4

ENTRY(boot2)
	data32
	xorl	%eax, %eax
	movl	%cs, %ax
	movl	%ax, %ds
	movl	%ax, %es
	addr32
	data32
	movl	%eax, _C_LABEL(ourseg)
	data32
	shll	$4, %eax

	/* fix up GDT entries for bootstrap */
#define FIXUP(gdt_index) \
	addr32; \
	movl	%eax, _C_LABEL(Gdt)+(8*gdt_index)+2;	/* actually movw %ax */ \
	addr32; \
	movb	%bl, _C_LABEL(Gdt)+(8*gdt_index)+4

	data32
	shldl	$16, %eax, %ebx
	FIXUP(BOOT_CS_INDEX)
	FIXUP(BOOT_CS16_INDEX)
	FIXUP(BOOT_DS_INDEX)

	/* fix up GDT pointer */
	data32
	addl	$_C_LABEL(Gdt), %eax
	addr32
	data32
	movl	%eax, _C_LABEL(Gdtr)+2

	/* change to protected mode */
	data32
	call	_C_LABEL(real_to_prot)

	/* clear the bss */
	movl	$_C_LABEL(edata), %edi
	movl	$_C_LABEL(end), %ecx
	subl	%edi, %ecx
	subb	%al, %al
	rep
	stosb

	movzbl	%dl, %edx	/* discard head (%dh) and random high bits */
	pushl	%edx
	call	_C_LABEL(boot)

stop:
	cli
	hlt


/* error messages */

#ifdef	DEBUG
one:	.asciz		"1\r\n"
two:	.asciz		"2\r\n"
three:	.asciz		"3\r\n"
four:	.asciz		"4\r\n"
five:	.asciz		"5\r\n"
six:	.asciz		"6\r\n"
seven:	.asciz		"7\r\n"
#endif	DEBUG
eread:	.asciz		"Read error\r\n"
enoboot: .asciz		"No bootable partition\r\n"
endofcode:
/* throw in a partition in case we are block0 as well */
/* flag, head, sec, cyl, typ, ehead, esect, ecyl, start, len */
	. = _C_LABEL(boot1) + PARTSTART
	.byte	0x0,0,0,0,0,0,0,0
	.long	0,0
	.byte	0x0,0,0,0,0,0,0,0
	.long	0,0
	.byte	0x0,0,0,0,0,0,0,0
	.long	0,0
	.byte	BOOTABLE,0,1,0,BSDPART,255,255,255
	.long	0,50000
/* the last 2 bytes in the sector 0 contain the signature */
	. = _C_LABEL(boot1) + 0x1fe
	.short	SIGNATURE
	. = _C_LABEL(boot1) + 0x200
	.globl	_disklabel
_disklabel:
	. = _C_LABEL(boot1) + 0x400	
