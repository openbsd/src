
	
	.section ".data"
	.comm	environ, 4
	.globl	__progname
__progname:
	.long L1
L1:	.long 0		# null string plus padding

	

	.section ".text"
	.globl start
start:	
	.globl _start
	.type	_start,@function
_start:	
	
	# squirrel away the arguments for main
	mr 13, 3
	mr 14, 4
	mr 15, 5
	mr 16, 6

	# make certain space exists on the stack for the
	# 'stw X, 4(1)' at the beginning of functions.
	subi	1, 1, 16
	li	0, 0
	stw	0, 0(1)

	# determine the program name (__progname)
	mr 4, 14
	lwz 28, 0(4)		# r28 = argv[0] (program name)
	cmpwi 28, 0		# argv[0] == NULL?
	beq call_main		# yep forget setup of progname
	mr 3, 28		# r3 = argv[0]
	li 4, '/'		# r4 = '/'
	.extern strrchr
	bl strrchr		# strrchr(argv[0], '/')
	cmpwi 3, 0		#    == 0?
	beq no_slash_found	# not found, use argv[0] still in r28
	addi 28, 3, 1		# was found, point at char after '/'
no_slash_found:
	# store the address of the basename found in argv[0]
	lis 31, __progname@HA
	stw 28, __progname@L(31)

	lis	26, environ@HA
	stw	15, environ@L(26)

	.globl __init
call_main:	
#ifdef MCRT0
	lis	3, _mcleanup@ha
	addi	3, 3, _mcleanup@l
	bl atexit
	lis	3, eprol@ha
	addi	3, 3, eprol@l
	lis	4, _etext@ha
	addi	4, 4, _etext@l
	bl monstartup
#endif
	bl __init
	# recover those saved registers
	mr 3, 13
	mr 4, 14
	mr 5, 15
	mr 6, 16
	
	bl main
	.extern exit
	bl exit

eprol:
#if 0
	.globl __main
__main:
#endif
	.globl __eabi
	.type  __eabi,@function
__eabi:
	blr

