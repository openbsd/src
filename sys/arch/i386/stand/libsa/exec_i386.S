/*	$OpenBSD: exec_i386.S,v 1.2 1997/03/31 23:08:37 mickey Exp $	*/

#include <machine/asm.h>

/*
 * machdep_start(phyaddr)
 *	start the program on protected mode where phyaddr is the entry point
 */
	.globl	_cnvmem, _extmem
ENTRY(machdep_start)
	pushl	%ebp
	movl	%esp, %ebp

	# get things we need into registers
	movl	8(%ebp), %ecx		# entry offset 
	movl	12(%ebp), %edi		# howto
	movl	16(%ebp), %edx		# loadaddr
	movl	24(%ebp), %esi		# esym

	# make a new stack at 0:0x90000 (big segs)
	movl	$0x10, %eax
	movw	%ax, %ss
	movl	$0x90000, %eax
	movl	%eax, %esp
	
	# push some number of args onto the stack
	pushl	_cnvmem
	pushl	_extmem
	pushl	%esi		# esym
	pushl	$0		# cyloff
	pushl	$0		# bootdev
	pushl	%edi		# howto
	pushl	$0		# dummy 'return' address
	
	# push on our entry address
	movl	0x8, %eax		# segment
	pushw	%ax
	andl	$0xffffff, %ecx
	pushl	%ecx

	# convert the PC (and code seg)
	lret
