	.file	1 "foo.cc"
	.version	"01.01"
	.set noat
gcc2_compiled.:
__gnu_compiled_cplusplus:
	.globl memcpy
.section	.rodata
	.align 3
$C36:
	.ascii "2 empty strings:\0"
	.align 3
$C37:
	.ascii "\12\0"
.text
	.align 3
	.globl main
	.ent main
main:
	ldgp $29,0($27)
main..ng:
	lda $30,-32($30)
	.frame $30,32,$26,0
	stq $26,0($30)
	stq $9,8($30)
	.mask 0x4000200,-32
	.prologue 1
	addq $30,16,$9
	lda $16,_t12basic_string2ZcZt18string_char_traits1Zc$nilRep
	ldq $1,24($16)
	beq $1,$530
	jsr $26,clone__Q2t12basic_string2ZcZt18string_char_traits1Zc3Rep
	ldgp $29,0($26)
	br $31,$529
	.align 4
$530:
	ldq $1,16($16)
	addq $1,1,$1
	stq $1,16($16)
	lda $0,_t12basic_string2ZcZt18string_char_traits1Zc$nilRep+32
$529:
	stq $0,0($9)
	lda $16,cout
	lda $17,$C36
	jsr $26,__ls__7ostreamPCc
	ldgp $29,0($26)
	bis $0,$0,$16
	addq $30,16,$17
	jsr $26,__ls__H2ZcZt18string_char_traits1Zc_R7ostreamRCt12basic_string2ZX01ZX11_R7ostream
	ldgp $29,0($26)
	bis $0,$0,$16
	addq $30,16,$17
	jsr $26,__ls__H2ZcZt18string_char_traits1Zc_R7ostreamRCt12basic_string2ZX01ZX11_R7ostream
	ldgp $29,0($26)
	bis $0,$0,$16
	lda $17,$C37
	jsr $26,__ls__7ostreamPCc
	ldgp $29,0($26)
	ldq $1,16($30)
	subq $1,32,$16
	ldq $1,16($16)
	subq $1,1,$1
	stq $1,16($16)
	bne $1,$534
	jsr $26,__builtin_delete
	ldgp $29,0($26)
$534:
	bis $31,$31,$0
	ldq $26,0($30)
	ldq $9,8($30)
	addq $30,32,$30
	ret $31,($26),1
	.end main
	.ident	"GCC: (GNU) egcs-2.90.15 971031 (gcc2-970802 experimental)"
