; ARM6 PSR transfer macros

; Condition code symbols

Cond_EQ *       0  :SHL: 28
Cond_NE *       1  :SHL: 28
Cond_CS *       2  :SHL: 28
Cond_HS * Cond_CS
Cond_CC *       3  :SHL: 28
Cond_LO * Cond_CC
Cond_MI *       4  :SHL: 28
Cond_PL *       5  :SHL: 28
Cond_VS *       6  :SHL: 28
Cond_VC *       7  :SHL: 28
Cond_HI *       8  :SHL: 28
Cond_LS *       9  :SHL: 28
Cond_GE *       10 :SHL: 28
Cond_LT *       11 :SHL: 28
Cond_GT *       12 :SHL: 28
Cond_LE *       13 :SHL: 28
Cond_AL *       14 :SHL: 28
Cond_   * Cond_AL
Cond_NV *       15 :SHL: 28

; New positions of I and F bits in 32-bit PSR

I32_bit *       1 :SHL: 7
F32_bit *       1 :SHL: 6
IF32_26Shift *  26-6

; Processor mode numbers

USR26_mode      *       2_00000
FIQ26_mode      *       2_00001
IRQ26_mode      *       2_00010
SVC26_mode      *       2_00011
USR32_mode      *       2_10000
FIQ32_mode      *       2_10001
IRQ32_mode      *       2_10010
SVC32_mode      *       2_10011
ABT32_mode      *       2_10111
UND32_mode      *       2_11011

; New register names

r13_abort       RN      13
r14_abort       RN      14
lr_abort        RN      14

r13_undef       RN      13
r14_undef       RN      14
lr_undef        RN      14

        MACRO
        mrs     $cond, $rd, $psrs
        LCLA    psrtype
psrtype SETA    -1
 [ "$psrs" = "CPSR" :LOR: "$psrs" = "CPSR_all"
psrtype SETA    0 :SHL: 22
 ]
 [ "$psrs" = "SPSR" :LOR: "$psrs" = "SPSR_all"
psrtype SETA    1 :SHL: 22
 ]
        ASSERT  psrtype <> -1
        ASSERT  $rd <> 15
        &       Cond_$cond :OR: 2_00000001000011110000000000000000 :OR: psrtype :OR: ($rd :SHL: 12)
        MEND

        MACRO
        msr     $cond, $psrl, $op2a, $op2b
        LCLA    psrtype
        LCLS    op2as
        LCLA    op
        LCLA    shift
psrtype SETA    -1
 [ "$psrl" = "CPSR" :LOR: "$psrl" = "CPSR_all"
psrtype SETA    (0:SHL:22) :OR: (1:SHL:19) :OR: (1:SHL:16)
 ]
 [ "$psrl" = "CPSR_flg"
psrtype SETA    (0:SHL:22) :OR: (1:SHL:19) :OR: (0:SHL:16)
 ]
 [ "$psrl" = "CPSR_ctl"
psrtype SETA    (0:SHL:22) :OR: (0:SHL:19) :OR: (1:SHL:16)
 ]
 [ "$psrl" = "SPSR" :LOR: "$psrl" = "SPSR_all"
psrtype SETA    (1:SHL:22) :OR: (1:SHL:19) :OR: (1:SHL:16)
 ]
 [ "$psrl" = "SPSR_flg"
psrtype SETA    (1:SHL:22) :OR: (1:SHL:19) :OR: (0:SHL:16)
 ]
 [ "$psrl" = "SPSR_ctl"
psrtype SETA    (1:SHL:22) :OR: (0:SHL:19) :OR: (1:SHL:16)
 ]
        ASSERT  psrtype <> -1
 [ ("$op2a" :LEFT: 1) = "#"
 ; Immediate operand

op2as   SETS    "$op2a" :RIGHT: ((:LEN: "$op2a")-1)
op      SETA    $op2as

  [ "$op2b" = ""
  ; Rotate not specified in immediate operand
shift   SETA    0
        WHILE   (op :AND: &FFFFFF00)<>0 :LAND: shift<16
op      SETA    ((op:SHR:30):AND:3):OR:(op:SHL:2)
shift   SETA    shift + 1
        WEND
        ASSERT  (op :AND: &FFFFFF00)=0
  |
  ; Rotate of immediate operand specified explicitly
        ASSERT  (($op2b):AND:&FFFFFFE1)=0
shift   SETA    ($opt2b):SHR:1
  ]
op      SETA    (shift :SHL: 8) :OR: op :OR: (1:SHL:25)
 |

 ; Not an immediate operand
  [ "$op2b" = ""
  ; Unshifted register
op      SETA    ($op2a) :OR: (0:SHL:25)
  |
        ! 1, "Shifted register not yet implemented in this macro!"
  ]
 ]
        &       Cond_$cond :OR: 2_00000001001000001111000000000000 :OR: op :OR: psrtype
        MEND

; SetMode newmode, reg1, regoldpsr
;
; Sets processor mode to constant value newmode
; using register reg1 as a temporary.
; If regoldpsr is specified, then this register
; on exit holds the old PSR before the mode change
; reg1 on exit always holds the new PSR after the mode change

;        MACRO
;        SetMode $newmode, $reg1, $regoldpsr
; [ "$regoldpsr"=""
;        mrs     AL, $reg1, CPSR_all
;        BIC     $reg1, $reg1, #&1F
;        ORR     $reg1, $reg1, #$newmode
;        msr     AL, CPSR_all, $reg1
; |
;        mrs     AL, $regoldpsr, CPSR_all
;        BIC     $reg1, $regoldpsr, #&1F
;        ORR     $reg1, $reg1, #$newmode
;        msr     AL, CPSR_all, $reg1
; ]
;        MEND

        MACRO
        mrc     $cond, $coproc, $op, $rd, $crn, $crm, $info
        &       Cond_$cond :OR: 2_00001110000100000000000000010000 :OR: ($coproc :SHL: 8) :OR: ($op :SHL: 21) :OR: ($rd :SHL: 12) :OR: ($crn :SHL: 16) :OR: $crm :OR: ($info :SHL: 5)
        MEND

        MACRO
        mcr     $cond, $coproc, $op, $rd, $crn, $crm, $info
        &       Cond_$cond :OR: 2_00001110000000000000000000010000 :OR: ($coproc :SHL: 8) :OR: ($op :SHL: 21) :OR: ($rd :SHL: 12) :OR: ($crn :SHL: 16) :OR: $crm :OR: ($info :SHL: 5)
        MEND

	END
