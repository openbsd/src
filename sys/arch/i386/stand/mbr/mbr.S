/*	$OpenBSD: mbr.S,v 1.3 1997/08/07 11:49:59 niklas Exp $	*/

/*
 * Copyright (c) 1997 Michael Shalayeff and Tobias Weingartner
 */

/* Copyright (c) 1996 VaX#n8 (vax@linkdead.paranoia.com)
 * last edited 9 July 1996
 * many thanks to Erich Boleyn (erich@uruk.org) for putting up with
 * all my questions, and for his work on GRUB
 * You may use this code or fragments thereof in a manner consistent
 * with the other copyrights as long as you retain my pseudonym and
 * this copyright notice in the file.
 */

	.file	"mbr.S"

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
#define BOOTABLE	0x80	/* bootable partition */

#ifdef DEBUG
#define CHAR_S		'S'	/* started */
#define CHAR_R		'R' /* relocated */
#define CHAR_L		'L'	/* looking for bootable partition */
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
	/* Adjust %cs to be right */
	data32
	ljmp 	$BOOTBIOS, $1f
1:
	/* Set up stack */
	movl	%cs, %ax
	cli
	movl	%ax, %ss
	data32
	movl	$0xfffc, %esp
	sti

	/* Set up data segment */
	movl	%ax, %ds
	DBGMSG(CHAR_S)

	/* Relocate 512 bytes so we can load PBS here  */
	data32
	movl	$BOOTRELOC, %eax
	movl	%ax, %es
	data32
	xorl	%si, %si
	data32
	xorl	%di, %di
	data32
	movl	$0x200, %ecx
	cld
	rep
	movsb

	/* Jump to relocated self */
	data32
	ljmp $BOOTRELOC, $reloc
reloc:
	DBGMSG(CHAR_R)

	/* Set up %es and %ds */
	pushl	%ds
	popl	%es	/* next boot is at the same place as we were loaded */
	pushl	%cs
	popl	%ds	/* and %ds is at the %cs */

#ifdef SERIAL
	/* Initialize the serial port to 9600 baud, 8N1. */
	 * Do we need to do this?  Most things at this level
	 * do not know or care (on a PC) where the output is
	 * happening to go.  I think if we are headless,
	 * /boot should figure (as it does now) that out.
	 *
	 * If there is a problem with this stage of the boot
	 * process, connect up a monitor and kbd, and see what
	 * is going on.  Left here for the time being.
	 *
	 * --Toby.
	 */
	xorl	%ax, %ax
	movb	$0xe3, %ax
	data32
	movl	$SERIAL, %dx
	int	$0x14
#endif

	/* bootstrap passes us drive number in %dl
	 *
	 * XXX - This is not always true.  We currently
	 * check if %dl points to a HD, and if not we
	 * complain, and set it to point to the first
	 * HDD.  Note, this is not 100% correct, since
	 * there is a possibility that you boot of of
	 * HD #2, and still get (%dl & 0x80) == 0x00,
	 * these type of systems will loose.  I don't
	 * know of any like this, but I've come to the
	 * conclusion, that if it can exist, it will,
	 * someplace in the PC world.  If anyone knows
	 * how to fix this, speak up!
	 *
	 * Toby - Thu Jul 31 21:01:00 CDT 1997
	 */
	testb	$0x80, %dl
	jnz	1f

	/* MBR on floppy or old BIOS
	 * Note: MBR (this code) should never
	 * be on a floppy.  It does not belong
	 * there, so %dl should never be 0x00.
	 *
	 * Here we simply complain (should we?),
	 * and then hardcode the boot drive to
	 * 0x80.
	 */
	data32
	movl	$fdmbr, %esi
	data32
	call	message

	/* If we are passed bogus data, set it to HD #1.
	 * We should load the value from a hard coded
	 * location in this sector.  Maybe I'll write
	 * that next, since my machines seem to be one
	 * of the weird ones...
	 */
	movb	$0x80, %dl

	/* Do we need to check our signature?  The BIOS will
	 * check it for us, I doubt there is a need for us to
	 * do the same thing over again.  If we fail here,
	 * something terrible is wrong.  However, I doubt we
	 * can recover anyways.  The message might be nice
	 * for the (l)user though.
	 */
1:	xorl	%bx, %bx
	# cmpw	$SIGNATURE, (%bx)
	.byte	0x81, 0xbf
	.word	signature
	.word	SIGNATURE
	je		sigok
	data32
	movl	$esig, %esi
	data32
	call	message

	/* find the first active partition
	 * Note: this should be the only active
	 * partition.  We currently don't check
	 * for that, but we really should.  If
	 * and when I feel up to it, I'll add
	 * that code.
	 */
sigok:
	data32
	movl	$pt, %esi
	data32
	movl	$NUMPART, %cx
1:
	DBGMSG(CHAR_L)
	# movb	(%si), %al
	.byte	0x8a, 0x44, 0x00
	cmpb	$BOOTABLE, %al
	je		found
	data32
	addl	$PARTSZ, %esi
	loop	1b

	/* No bootable partition */
no_part:
	data32
	movl	$noboot, %esi
	data32
	call	message
err_stop:
	cli
	hlt
	/* Just to make sure */
	jmp		err_stop

	/* Found bootable partition */
found:
	DBGMSG(CHAR_B)
	pushl	%eax
	pushl	%edx
	pushl	%esi
	pushl	%ecx

	/* Print out drive and partition */
	data32
	movl	$drive, %esi
	data32
	call	message

	movl	%dx, %ax
	andl	$0x0F, %ax
	orl		$0x30, %ax
	data32
	call	chr

	data32
	movl	$partn, %esi
	data32
	call	message

	popl	%eax
	andl	$0x0F, %ax
	orl		$0x30, %ax
	data32
	call	chr

	data32
	movl	$newln, %esi
	data32
	call	message

	/* Restore %esi and %edx */
	popl	%esi
	popl	%edx
	popl	%eax

	/* Load values from active partition table entry */
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
	jnc		1f
	data32
	movl    $eread, %si
	data32
	call	message
	jmp		err_stop


1:
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
	popl	%eax
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

/* Info messages */
drive:	.asciz		"Using Drive: "
partn:	.asciz		" Partition: "
newln:	.asciz		"\r\n"

/* Error messages */
fdmbr:	.asciz		"MBR on floppy or old BIOS\r\n"
eread:	.asciz		"Read error\r\n"
noboot: .asciz		"No active partition\r\n"
esig:	.asciz		"Invalid Signature\r\n"

endofcode:
	nop

/* (MBR) NT registry offset */
	. = 0x1b8
	.space  4, 0

/* partition table */
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
	.long	0,0x7FFFFFFF
/* the last 2 bytes in the sector 0 contain the signature */
	. = 0x1fe
signature:
	.short	SIGNATURE
	. = 0x200

