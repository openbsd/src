;******************************************************************************
;* Copyright 1991 Advanced Micro Devices, Inc.
;*
;* This software is the property of Advanced Micro Devices, Inc  (AMD)  which
;* specifically  grants the user the right to modify, use and distribute this
;* software provided this notice is not removed or altered.  All other rights
;* are reserved by AMD.
;*
;* AMD MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARD TO THIS
;* SOFTWARE.  IN NO EVENT SHALL AMD BE LIABLE FOR INCIDENTAL OR CONSEQUENTIAL
;* DAMAGES IN CONNECTION WITH OR ARISING FROM THE FURNISHING, PERFORMANCE, OR
;* USE OF THIS SOFTWARE.
;*
;* Comments about this software should be directed to udi@amd.com. If access
;* to electronic mail isn't available, send mail to:
;*
;* Advanced Micro Devices, Inc.
;* 29K Support Products
;* Mail Stop 573
;* 5900 E. Ben White Blvd.
;* Austin, TX 78741
;*****************************************************************************
;*       $Id: dosdfe.asm,v 1.2 1996/11/23 04:11:15 niklas Exp $
;*	 $Id: @(#)dosdfe.asm	2.5, AMD
;*
IFNDEF	DOS386
	DOSSEG
ENDIF
	INCLUDE udidos.ah
IFDEF	DOS386

rmcode	segment byte public use16
rmcode	ends
rmdata	segment dword public use16
rmdata	ends
pmdata	segment dword public use32
pmdata	ends
pmcode	segment byte public use32
pmcode	ends
dgroup	group	rmdata,pmdata



rmcode	segment

;
; Symbol marking start of real mode code & data,
; used at link time to specify the real mode
; code & data size
;
	public	start_real
start_real label	byte

rmcode	ends



;
; Data that needs to be accessed in both real
; mode and protected mode
;
rmdata	segment


	public code_selector, data_selector, call_prot
	; these get filled in by protected mode startup code

code_selector  DW ?
data_selector  DW ?
call_prot	DD ?

rmdata	ends




rmdata	segment

	public	TermStruct		; No auto underscore for Watcom C or HighC386
TermStruct	DOSTerm <>		; Don't initialize, it will get filled at run time.
	public  UDITerminate		; need this so we can set up real addr into TermSTruct

rmdata	ends

rmcode	segment


ELSE	; not DOS386

	PUBLIC	_TermStruct
	.MODEL	LARGE
	.DATA	
_TermStruct  DOSTerm <UDITerminate>
	.CODE

ENDIF  ; DOS386




UDITerminate	PROC	FAR
;
; Retrieve registers from save area
IFDEF	DOS386
	ASSUME  CS:rmcode
	mov	bx, OFFSET TermStruct	; in 386 mode, the pointer we pass to TIP
					; has UDITerminate seg = seg(rmcode)
	mov	ax, cs
ELSE	; not DOS386
	mov	bx, OFFSET _TermStruct
	mov	ax, DGROUP
ENDIF	; DOS386
	mov	es, ax
	mov	ss, es:[bx].sss
	mov	sp, es:[bx].ssp
	mov	ds, es:[bx].sds
	mov	si, es:[bx].ssi
	mov	di, es:[bx].sdi
	mov	bp, es:[bx].sbp
	mov	ax, es:[bx].retval
	ret				; far return because of PROC FAR

UDITerminate ENDP

IFDEF	DOS386

rmcode	ends



pmcode segment


	ASSUME CS:pmcode
	ASSUME DS:dgroup

	public	GetCS
GetCS	PROC NEAR
	mov	ax, cs
	ret
GetCS	ENDP

	public	GetDS
GetDS	PROC NEAR
	mov	ax, ds
	ret
GetDS	ENDP


	public	_exp_call_to
_exp_call_to PROC NEAR
	push	ebp
	mov	ebp, esp
	push	es		; save at least all regs required by hc386
	push 	gs
	push	fs
	push	ds	
	push	ebx
	push	esi
	push	edi
	mov	ebx, [ebp+8]
	mov	ax, [ebx+0ah]	; new ss,ds,etc.
	mov	ecx, [ebx+6]	; new sp
	sub	ecx, 256	; back up past TIPname space
	mov	es, ax
	mov	gs, ax
	mov	fs, ax
	mov	ds, ax
	mov	edx, esp
	mov	si, ss	; save old ss:sp
	mov	ss, ax
	mov	esp, ecx
	; now on new stack, save old stack
	push	edx
	push	si
	call	fword ptr cs:[ebx]
	; restore old stack
	pop	si
	pop	edx
	mov	ss,si
	mov	esp,edx
	; now we are back on original stack
	pop	edi
	pop	esi
	pop	ebx
	pop	ds
	pop	fs
	pop	gs
	pop	es
	pop	ebp
	ret			; eax will be the return

_exp_call_to ENDP


pmcode ends



rmdata   segment
;
; Symbol marking end of real mode code & data,
; used at link time to specify the real mode
; code & data size
;
	public	end_real
end_real label	byte
rmdata   ends

ENDIF	; DOS386

	END
