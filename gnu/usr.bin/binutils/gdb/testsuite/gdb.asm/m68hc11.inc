	comment "subroutine prologue"
	.macro gdbasm_enter
        ldx     _.frame
	pshx
	sts     _.frame
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
        pulx
        stx     _.frame
	rts
	.endm

	.macro gdbasm_call subr
	jsr	\subr
	.endm

	.macro gdbasm_several_nops
	nop
	nop
	nop
	nop
	.endm

	comment "exit (0)"
	.macro gdbasm_exit0
        clra
        clrb
        wai
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
        .sect .data
        .globl _.frame
_.frame: .word 0
        .previous
        lds     #0x2000
        clr     _.frame
        clr     _.frame+1
	.endm

	comment "Declare a data variable"
	.macro gdbasm_datavar name value
	.data
\name:
	.long	\value
	.endm
