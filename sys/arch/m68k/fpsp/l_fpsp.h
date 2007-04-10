*	$OpenBSD: l_fpsp.h,v 1.3 2007/04/10 17:47:54 miod Exp $
*	$NetBSD: l_fpsp.h,v 1.2 1994/10/26 07:49:14 cgd Exp $

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
*	l_fpsp.h 1.2 5/1/91
*

*	l_fpsp.h --- stack frame offsets for library version of FPSP
*
*	This file is derived from fpsp.h.  All equates that refer
*	to the fsave frame and its bits are removed with the
*	exception of ETEMP, WBTEMP, DTAG and STAG which are simulated
*	in the library version.  Equates for the exception frame are
*	also not needed.  Some of the equates that are only used in
*	the kernel version of the FPSP are left in to minimize the
*	differences between this file and the original.
*
*	The library routines use the same source files as the regular
*	kernel mode code so they expect the same setup.  That is, you
*	must create enough space on the stack for all save areas and
*	work variables that are needed, and save any registers that
*	your compiler does not treat as scratch registers on return
*	from function calls.
*	
*	The worst case setup is:
*
*		link	a6,#-LOCAL_SIZE
*		movem.l	d0-d1/a0-a1,USER_DA(a6)
*		fmovem.x fp0-fp3,USER_FP0(a6)
*		fmovem.l fpsr/fpcr,USER_FPSR(a6)
*
*	After initialization, the stack looks like this:
*
*	A7 --->	+-------------------------------+
*		|				|
*		|	FPSP Local Variables	|
*		|	     including		|
*		|	  saved registers	|
*		|				|
*		+-------------------------------+
*	A6 --->	|	Saved A6		|
*		+-------------------------------+
*		|	Return PC		|
*		+-------------------------------+
*		|	Arguments to 		|
*		|	an FPSP library		|
*		|	package			|
*		|				|
*
*	Positive offsets from A6 refer to the input arguments.  Negative
*	offsets refer to the Local Variable area.
*
*	On exit, execute:
*
*		movem.l	USER_DA(a6),d0-d1/a0-a1
*		fmovem.x USER_FP0(a6),fp0-fp3
*		fmove.l	USER_FPSR(a6),fpsr/fpcr
*		unlk	a6
*		rts
*
*	Many 68K C compilers treat a0/a1/d0/d1/fp0/fp1 as scratch so
*	a simplified setup/exit is possible:
*
*		link	a6,#-LOCAL_SIZE
*		fmovem.x fp2-fp3,USER_FP2(a6)
*		fmove.l	fpsr/fpcr,USER_FPSR(a6)
*
*		[call appropriate emulation routine]
*
*		fmovem.x USER_FP2(a6),fp2-fp3
*		fmove.l	USER_FPSR(a6),fpsr/fpcr
*		unlk	a6
*		rts
*
*	Note that you must still save fp2/fp3 because the FPSP emulation
*	routines expect fp0-fp3 as scratch registers.  For all monadic
*	entry points, the caller should save the fpcr in d1 and zero the
*	real fpcr before calling the emulation routine.  On return, the
*	monadic emulation code will place the value supplied in d1 back
*	into the fpcr and do a single floating point operation so that
*	the final result will be correctly rounded and any specified
*	exceptions will be generated.
*
*----------------------------------------------------------------------
*
*	Local Variables on the stack
*
LOCAL_SIZE	equ	228		;bytes needed for local variables
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
*--------------------------------------------------------------------------
*
STAG		equ	LV+192		;source tag (1 byte)
*
DTAG		equ	LV+193		;dest tag (1 byte)
*
FPTEMP		equ	LV+196		;fptemp (12 bytes)
FPTEMP_EX	equ	FPTEMP		;fptemp sign and exponent (2 bytes)
FPTEMP_HI	equ	FPTEMP+4	;fptemp mantissa [63:32] (4 bytes)
FPTEMP_LO	equ	FPTEMP+8	;fptemp mantissa [31:00] (4 bytes)
*
FPTEMP_SGN	equ	FPTEMP+2	;used to store sign
*
ETEMP		equ	LV+208		;etemp (12 bytes)
ETEMP_EX	equ	ETEMP		;etemp sign and exponent (2 bytes)
ETEMP_HI	equ	ETEMP+4		;etemp mantissa [63:32] (4 bytes)
ETEMP_LO	equ	ETEMP+8		;etemp mantissa [31:00] (4 bytes)
*
ETEMP_SGN	equ	ETEMP+2		;used to store sign
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
dbl_thresh	equ	$3C01
sgl_thresh	equ	$3F81
*
