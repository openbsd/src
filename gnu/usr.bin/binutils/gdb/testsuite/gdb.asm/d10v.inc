	comment "subroutine prologue"
	.macro gdbasm_enter
	st r11,@-sp
	st r13,@-sp
	mv r11,sp
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	add3 sp,r11,0
	ld   r13,@sp+
	ld   r11,@sp+
	jmp  r13
	.endm

	.macro gdbasm_call subr
	bl \subr
	.endm

	.macro gdbasm_several_nops
	nop
	nop
	nop
	nop
	.endm

	comment "exit (0)"
	.macro gdbasm_exit0
	ldi r4, 1
	ldi r0, 0
	trap 15
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
; R14 always contains memory base address (0)

	ldi     r14,0

; Set the USER and SYSTEM stack pointers.

	ldi     r0, 0           ; zero arguments
	ldi     r1, 0
	mvtc    r0, psw         ; select SPI and set it
	ldi     sp, _stack
	ldi     r10, 0x8000     ; select SPU/FP and set it
	mvtc	r10, psw || ldi	r11, 0;  clear stack frame
	ldi     sp, _stack - 0x200
	ldi     r13, 0

	st	r11, @-sp
	st	r13, @-sp
;	mv	r11, sp

	.endm
