	.SPACE $PRIVATE$
	.SUBSPA $DATA$,QUAD=1,ALIGN=8,ACCESS=31
	.SUBSPA $BSS$,QUAD=1,ALIGN=8,ACCESS=31,ZERO,SORT=82
	.SPACE $TEXT$
	.SUBSPA $LIT$,QUAD=0,ALIGN=8,ACCESS=44
	.SUBSPA $CODE$,QUAD=0,ALIGN=8,ACCESS=44,CODE_ONLY

	.SPACE $TEXT$
	.SUBSPA $CODE$

	.align 1
	.align 8
	nop
; "8" assumed if no alignment given.
	.align
	nop
	.align 4096
	nop


        .SPACE  $PRIVATE$
        .SUBSPA $BSS$

        .ALIGN  8
$L00BSS:
home_buff:
        .BLOCK  1024
        .ALIGN  8
current_buff:
        .BLOCK  1024
        .ALIGN  4
lock_file:
        .BLOCK  4
        .ALIGN  8
L332.name:
        .BLOCK  30
        .ALIGN  4
L352.last_case_wa:
        .BLOCK  4


