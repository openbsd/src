
	
	.data 1
	.comm	environ, 4
	.globl	__progname
__progname:
	.long L1
L1:	.long 0		# null string plus padding

	

	.text
	.globl start
start:	
	.globl _start
_start:	
	
	# squirrel away the arguments for main
	mr 13, 3
	mr 14, 4
	mr 15, 5
	mr 16, 6

	# determine the program name (__progname)
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
	stw	5, environ@L(26)

call_main:	
	.globl __init
	bl __init
	# recover those saved registers
	mr 3, 13
	mr 4, 14
	mr 5, 15
	mr 6, 16
	
	bl main
	.extern exit
	mr 13, 3
	bl __fini
	mr 3, 13
	bl exit

	.globl __main
__main:
	.globl __eabi
__eabi:
	blr

