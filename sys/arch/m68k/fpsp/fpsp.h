*	$OpenBSD: fpsp.h,v 1.3 2007/11/27 16:22:13 martynas Exp $
*	$NetBSD: fpsp.h,v 1.2 1994/10/26 07:49:04 cgd Exp $

*	MOTOROLA MICROPROCESSOR & MEMORY TECHNOLOGY GROUP
*	M68000 Hi-Performance Microprocessor Division
*	M68040 Software Package 
*
*	M68040 Software Package Copyright (c) 1993, 1994 Motorola Inc.
*	All rights reserved.
*
*	THE SOFTWARE is provided on an "AS IS" basis and without warranty.
*	To the maximum extent permitted by applicable law,
*	MOTOROLA DISCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLIED,
*	INCLUDING IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A
*	PARTICULAR PURPOSE and any warranty against infringement with
*	regard to the SOFTWARE (INCLUDING ANY MODIFIED VERSIONS THEREOF)
*	and any accompanying written materials. 
*
*	To the maximum extent permitted by applicable law,
*	IN NO EVENT SHALL MOTOROLA BE LIABLE FOR ANY DAMAGES WHATSOEVER
*	(INCLUDING WITHOUT LIMITATION, DAMAGES FOR LOSS OF BUSINESS
*	PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR
*	OTHER PECUNIARY LOSS) ARISING OF THE USE OR INABILITY TO USE THE
*	SOFTWARE.  Motorola assumes no responsibility for the maintenance
*	and support of the SOFTWARE.  
*
*	You are hereby granted a copyright license to use, modify, and
*	distribute the SOFTWARE so long as this entire notice is retained
*	without alteration in any modified and/or redistributed versions,
*	and that such modified versions are clearly identified as such.
*	No licenses are granted by implication, estoppel or otherwise
*	under any patents or trademarks of Motorola, Inc.

*
*	fpsp.h 3.3 3.3
*

*	fpsp.h --- stack frame offsets during FPSP exception handling
*
*	These equates are used to access the exception frame, the fsave
*	frame and any local variables needed by the FPSP package.
*	
*	All FPSP handlers begin by executing:
*
*		link	a6,#-LOCAL_SIZE
*		fsave	-(a7)
*		movem.l	d0-d1/a0-a1,USER_DA(a6)
*		fmovem.x fp0-fp3,USER_FP0(a6)
*		fmove.l	fpsr/fpcr/fpiar,USER_FPSR(a6)
*
*	After initialization, the stack looks like this:
*
*	A7 --->	+-------------------------------+
*		|				|
*		|	FPU fsave area		|
*		|				|
*		+-------------------------------+
*		|				|
*		|	FPSP Local Variables	|
*		|	     including		|
*		|	  saved registers	|
*		|				|
*		+-------------------------------+
*	A6 --->	|	Saved A6		|
*		+-------------------------------+
*		|				|
*		|	Exception Frame		|
*		|				|
*		|				|
*
*	Positive offsets from A6 refer to the exception frame.  Negative
*	offsets refer to the Local Variable area and the fsave area.
*	The fsave frame is also accessible 'from the top' via A7.
*
*	On exit, the handlers execute:
*
*		movem.l	USER_DA(a6),d0-d1/a0-a1
*		fmovem.x USER_FP0(a6),fp0-fp3
*		fmove.l	USER_FPSR(a6),fpsr/fpcr/fpiar
*		frestore (a7)+
*		unlk	a6
*
*	and then either 'bra fpsp_done' if the exception was completely
*	handled	by the package, or 'bra real_xxxx' which is an external
*	label to a routine that will process a real exception of the
*	type that was generated.  Some handlers may omit the 'frestore'
*	if the FPU state after the exception is idle.
*
*	Sometimes the exception handler will transform the fsave area
*	because it needs to report an exception back to the user.  This
*	can happen if the package is entered for an unimplemented float
*	instruction that generates (say) an underflow.  Alternatively,
*	a second fsave frame can be pushed onto the stack and the
*	handler	exit code will reload the new frame and discard the old.
*
*	The registers d0, d1, a0, a1 and fp0-fp3 are always saved and
*	restored from the 'local variable' area and can be used as
*	temporaries.  If a routine needs to change any
*	of these registers, it should modify the saved copy and let
*	the handler exit code restore the value.
*
*----------------------------------------------------------------------
*
*	Local Variables on the stack
*
LOCAL_SIZE	equ	192		;bytes needed for local variables
LV		equ	-LOCAL_SIZE	;convenient base value
*
USER_DA		equ	LV+0		;save space for D0-D1,A0-A1
USER_D0		equ	LV+0		;saved user D0
USER_D1		equ	LV+4		;saved user D1
USER_A0		equ	LV+8		;saved user A0
USER_A1		equ	LV+12		;saved user A1
USER_FP0	equ	LV+16		;saved user FP0
USER_FP1	equ	LV+28		;saved user FP1
USER_FP2	equ	LV+40		;saved user FP2
USER_FP3	equ	LV+52		;saved user FP3
USER_FPCR	equ	LV+64		;saved user FPCR
FPCR_ENABLE	equ	USER_FPCR+2	;	FPCR exception enable 
FPCR_MODE	equ	USER_FPCR+3	;	FPCR rounding mode control
USER_FPSR	equ	LV+68		;saved user FPSR
FPSR_CC		equ	USER_FPSR+0	;	FPSR condition code
FPSR_QBYTE	equ	USER_FPSR+1	;	FPSR quotient
FPSR_EXCEPT	equ	USER_FPSR+2	;	FPSR exception
FPSR_AEXCEPT	equ	USER_FPSR+3	;	FPSR accrued exception
USER_FPIAR	equ	LV+72		;saved user FPIAR
FP_SCR1		equ	LV+76		;room for a temporary float value
FP_SCR2		equ	LV+92		;room for a temporary float value
L_SCR1		equ	LV+108		;room for a temporary long value
L_SCR2		equ	LV+112		;room for a temporary long value
STORE_FLG	equ	LV+116
BINDEC_FLG	equ	LV+117		;used in bindec
DNRM_FLG	equ	LV+118		;used in res_func
RES_FLG		equ	LV+119		;used in res_func
DY_MO_FLG	equ	LV+120		;dyadic/monadic flag
UFLG_TMP	equ	LV+121		;temporary for uflag errata
CU_ONLY		equ	LV+122		;cu-only flag
VER_TMP		equ	LV+123		;temp holding for version number
L_SCR3		equ	LV+124		;room for a temporary long value
FP_SCR3		equ	LV+128		;room for a temporary float value
FP_SCR4		equ	LV+144		;room for a temporary float value
FP_SCR5		equ	LV+160		;room for a temporary float value
FP_SCR6		equ	LV+176
*
*NEXT		equ	LV+192		;need to increase LOCAL_SIZE
*
*--------------------------------------------------------------------------
*
*	fsave offsets and bit definitions
*
*	Offsets are defined from the end of an fsave because the last 10
*	words of a busy frame are the same as the unimplemented frame.
*
CU_SAVEPC	equ	LV-92		;micro-pc for CU (1 byte)
FPR_DIRTY_BITS	equ	LV-91		;fpr dirty bits
*
WBTEMP		equ	LV-76		;write back temp (12 bytes)
WBTEMP_EX	equ	WBTEMP		;wbtemp sign and exponent (2 bytes)
WBTEMP_HI	equ	WBTEMP+4	;wbtemp mantissa [63:32] (4 bytes)
WBTEMP_LO	equ	WBTEMP+8	;wbtemp mantissa [31:00] (4 bytes)
*
WBTEMP_SGN	equ	WBTEMP+2	;used to store sign
*
FPSR_SHADOW	equ	LV-64		;fpsr shadow reg
*
FPIARCU		equ	LV-60		;Instr. addr. reg. for CU (4 bytes)
*
CMDREG2B	equ	LV-52		;cmd reg for machine 2
CMDREG3B	equ	LV-48		;cmd reg for E3 exceptions (2 bytes)
*
NMNEXC		equ	LV-44		;NMNEXC (unsup,snan bits only)
nmn_unsup_bit	equ	1	
nmn_snan_bit	equ	0	
*
NMCEXC		equ	LV-43		;NMNEXC & NMCEXC
nmn_operr_bit	equ	7
nmn_ovfl_bit	equ	6
nmn_unfl_bit	equ	5
nmc_unsup_bit	equ	4
nmc_snan_bit	equ	3
nmc_operr_bit	equ	2
nmc_ovfl_bit	equ	1
nmc_unfl_bit	equ	0
*
STAG		equ	LV-40		;source tag (1 byte)
WBTEMP_GRS	equ	LV-40		;alias wbtemp guard, round, sticky
guard_bit	equ	1		;guard bit is bit number 1
round_bit	equ	0		;round bit is bit number 0
stag_mask	equ	$E0		;upper 3 bits are source tag type
denorm_bit	equ	7		;bit determines if denorm or unnorm
etemp15_bit	equ	4		;etemp exponent bit #15
wbtemp66_bit	equ	2		;wbtemp mantissa bit #66
wbtemp1_bit	equ	1		;wbtemp mantissa bit #1
wbtemp0_bit	equ	0		;wbtemp mantissa bit #0
*
STICKY		equ	LV-39		;holds sticky bit
sticky_bit	equ	7
*
CMDREG1B	equ	LV-36		;cmd reg for E1 exceptions (2 bytes)
kfact_bit	equ	12		;distinguishes static/dynamic k-factor
*					;on packed move out's.  NOTE: this
*					;equate only works when CMDREG1B is in
*					;a register.
*
CMDWORD		equ	LV-35		;command word in cmd1b
direction_bit	equ	5		;bit 0 in opclass
size_bit2	equ	12		;bit 2 in size field
*
DTAG		equ	LV-32		;dest tag (1 byte)
dtag_mask	equ	$E0		;upper 3 bits are dest type tag
fptemp15_bit	equ	4		;fptemp exponent bit #15
*
WB_BYTE		equ	LV-31		;holds WBTE15 bit (1 byte)
wbtemp15_bit	equ	4		;wbtemp exponent bit #15
*
E_BYTE		equ	LV-28		;holds E1 and E3 bits (1 byte)
E1		equ	2		;which bit is E1 flag
E3		equ	1		;which bit is E3 flag
SFLAG		equ	0		;which bit is S flag
*
T_BYTE		equ	LV-27		;holds T and U bits (1 byte)
XFLAG		equ	7		;which bit is X flag
UFLAG		equ	5		;which bit is U flag
TFLAG		equ	4		;which bit is T flag
*
FPTEMP		equ	LV-24		;fptemp (12 bytes)
FPTEMP_EX	equ	FPTEMP		;fptemp sign and exponent (2 bytes)
FPTEMP_HI	equ	FPTEMP+4	;fptemp mantissa [63:32] (4 bytes)
FPTEMP_LO	equ	FPTEMP+8	;fptemp mantissa [31:00] (4 bytes)
*
FPTEMP_SGN	equ	FPTEMP+2	;used to store sign
*
ETEMP		equ	LV-12		;etemp (12 bytes)
ETEMP_EX	equ	ETEMP		;etemp sign and exponent (2 bytes)
ETEMP_HI	equ	ETEMP+4		;etemp mantissa [63:32] (4 bytes)
ETEMP_LO	equ	ETEMP+8		;etemp mantissa [31:00] (4 bytes)
*
ETEMP_SGN	equ	ETEMP+2		;used to store sign
*
EXC_SR		equ	4		;exception frame status register
EXC_PC		equ	6		;exception frame program counter
EXC_VEC		equ	10		;exception frame vector (format+vector#)
EXC_EA		equ	12		;exception frame effective address
*
*--------------------------------------------------------------------------
*
*	FPSR/FPCR bits
*
neg_bit		equ	3	negative result
z_bit		equ	2	zero result
inf_bit		equ	1	infinity result
nan_bit		equ	0	not-a-number result
*
q_sn_bit	equ	7	sign bit of quotient byte
*
bsun_bit	equ	7	branch on unordered
snan_bit	equ	6	signalling nan
operr_bit	equ	5	operand error
ovfl_bit	equ	4	overflow
unfl_bit	equ	3	underflow
dz_bit		equ	2	divide by zero
inex2_bit	equ	1	inexact result 2
inex1_bit	equ	0	inexact result 1
*
aiop_bit	equ	7	accrued illegal operation
aovfl_bit	equ	6	accrued overflow
aunfl_bit	equ	5	accrued underflow
adz_bit		equ	4	accrued divide by zero
ainex_bit	equ	3	accrued inexact
*
*	FPSR individual bit masks
*
neg_mask	equ	$08000000
z_mask		equ	$04000000
inf_mask	equ	$02000000
nan_mask	equ	$01000000
*
bsun_mask	equ	$00008000	
snan_mask	equ	$00004000
operr_mask	equ	$00002000
ovfl_mask	equ	$00001000
unfl_mask	equ	$00000800
dz_mask		equ	$00000400
inex2_mask	equ	$00000200
inex1_mask	equ	$00000100
*
aiop_mask	equ	$00000080	accrued illegal operation
aovfl_mask	equ	$00000040	accrued overflow
aunfl_mask	equ	$00000020	accrued underflow
adz_mask	equ	$00000010	accrued divide by zero
ainex_mask	equ	$00000008	accrued inexact
*
*	FPSR combinations used in the FPSP
*
dzinf_mask	equ	inf_mask+dz_mask+adz_mask
opnan_mask	equ	nan_mask+operr_mask+aiop_mask
nzi_mask	equ	$01ffffff 	clears N, Z, and I
unfinx_mask	equ	unfl_mask+inex2_mask+aunfl_mask+ainex_mask
unf2inx_mask	equ	unfl_mask+inex2_mask+ainex_mask
ovfinx_mask	equ	ovfl_mask+inex2_mask+aovfl_mask+ainex_mask
inx1a_mask	equ	inex1_mask+ainex_mask
inx2a_mask	equ	inex2_mask+ainex_mask
snaniop_mask	equ	nan_mask+snan_mask+aiop_mask
naniop_mask	equ	nan_mask+aiop_mask
neginf_mask	equ	neg_mask+inf_mask
infaiop_mask	equ	inf_mask+aiop_mask
negz_mask	equ	neg_mask+z_mask
opaop_mask	equ	operr_mask+aiop_mask
unfl_inx_mask	equ	unfl_mask+aunfl_mask+ainex_mask
ovfl_inx_mask	equ	ovfl_mask+aovfl_mask+ainex_mask
*
*--------------------------------------------------------------------------
*
*	FPCR rounding modes
*
x_mode		equ	$00	round to extended
s_mode		equ	$40	round to single
d_mode		equ	$80	round to double
*
rn_mode		equ	$00	round nearest
rz_mode		equ	$10	round to zero
rm_mode		equ	$20	round to minus infinity
rp_mode		equ	$30	round to plus infinity
*
*--------------------------------------------------------------------------
*
*	Miscellaneous equates
*
signan_bit	equ	6	signalling nan bit in mantissa
sign_bit	equ	7
*
rnd_stky_bit	equ	29	round/sticky bit of mantissa
*				this can only be used if in a data register
sx_mask		equ	$01800000 set s and x bits in word $48
*
LOCAL_EX	equ	0
LOCAL_SGN	equ	2
LOCAL_HI	equ	4
LOCAL_LO	equ	8
LOCAL_GRS	equ	12	valid ONLY for FP_SCR1, FP_SCR2
*
*
norm_tag	equ	$00	tag bits in {7:5} position
zero_tag	equ	$20
inf_tag		equ	$40
nan_tag		equ	$60
dnrm_tag	equ	$80
*
*	fsave sizes and formats
*
VER_4		equ	$40		fpsp compatible version numbers
*					are in the $40s {$40-$4f}
VER_40		equ	$40		original version number
VER_41		equ	$41		revision version number
*
BUSY_SIZE	equ	100		size of busy frame
BUSY_FRAME	equ	LV-BUSY_SIZE	start of busy frame
*
UNIMP_40_SIZE	equ	44		size of orig unimp frame
UNIMP_41_SIZE	equ	52		size of rev unimp frame
*
IDLE_SIZE	equ	4		size of idle frame
IDLE_FRAME	equ	LV-IDLE_SIZE	start of idle frame
*
*	exception vectors
*
TRACE_VEC	equ	$2024		trace trap
FLINE_VEC	equ	$002C		'real' F-line
UNIMP_VEC	equ	$202C		unimplemented
INEX_VEC	equ	$00C4
*
dbl_thresh	equ	$3C01
sgl_thresh	equ	$3F81
*
