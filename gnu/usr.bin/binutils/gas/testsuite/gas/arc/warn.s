; Test ARC specific assembler warnings
;
; { dg-do assemble { target arc-*-* } }

	b.d foo
	mov r0,256	; { dg-warning "8 byte instruction in delay slot" "8 byte insn in delay slot" }

	j.d foo		; { dg-warning "8 byte jump instruction with delay slot" "8 byte jump with delay slot" }
	mov r0,r1

	sub.f 0,r0,r2
	beq foo		; { dg-warning "conditional branch follows set of flags" "cc set/branch nop test" }

foo:
