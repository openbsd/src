	.globl _print_str,_print_num

Lprint_str:	jmp _print_str
Lprint_num:	jmp _print_num

_060FPSP_TEST:
        .long    Lprint_str - _060FPSP_TEST      
	.long    Lprint_num - _060FPSP_TEST
	.space   120
#include "ftest.S"

_060ISP_TEST:
        .long    Lprint_str - _060ISP_TEST      
	.long    Lprint_num - _060ISP_TEST
	.space   120
#include "itest.S"

	.globl	_itest,_ftest1,_ftest2,_ftest3
_itest:
	bra	_060ISP_TEST+128+0
_ftest1:
	bra	_060FPSP_TEST+128+0
_ftest2:
	bra	_060FPSP_TEST+128+8
_ftest3:
	bra	_060FPSP_TEST+128+16

