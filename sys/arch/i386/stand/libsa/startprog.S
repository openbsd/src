/*	$OpenBSD: startprog.S,v 1.2 1997/03/31 05:52:25 weingart Exp $	*/

#include <machine/asm.h>


/*
 * startprog(phyaddr, argv)
 *	start the program on protected mode where phyaddr is the entry point
 */
ENTRY(startprog)
	pushl	%ebp
	movl	%esp, %ebp

	# get things we need into registers
	movl	8(%ebp), %ecx		# entry offset 
	movl	12(%ebp), %eax		# &argv

	# make a new stack at 0:0x90000 (big segs)
	movl	$0x10, %ebx
	movw	%bx, %ss
	movl	$0x90000, %ebx
	movl	%ebx, %esp

	# push some number of args onto the stack
	pushl	28(%eax)		# argv[7] = cnvmem
	pushl	32(%eax)		# argv[8] = extmem
	pushl	16(%eax)		# argv[4] = esym
	pushl	12(%eax)		# argv[3] = cyl offset
	pushl	8(%eax)			# argv[2] = bootdev
	pushl	4(%eax)			# argv[1] = howto
	pushl	$0			# dummy 'return' address

	# push on our entry address
	movl	$0x8, %ebx		# segment
	pushl	%bx
	pushl	%ecx

	# convert over the other data segs
	movl	$0x10, %ebx
	movl	%bx, %ds
	movl	%bx, %es

	# convert the PC (and code seg)
	lret

