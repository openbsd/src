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
;* 800-292-9263
;*****************************************************************************
;*       $Id: dostip.asm,v 1.2 1996/11/23 04:11:16 niklas Exp $
;*	 $Id: @(#)dostip.asm	2.7, AMD
;******************************************************************************
;*/

IFNDEF DOS386
	DOSSEG
	.MODEL	LARGE
	PUBLIC	_UDIASMConnect
	PUBLIC	_UDIASMDisconnect
	EXTRN	_UDICDisconnect:FAR
	EXTRN	_UDICConnect:FAR
	.CODE
ELSE
;
; Segment ordering and attributes for DOS386.  We make sure the
; real mode code and data comes first.  The real and
; prot mode data are together so they can be grouped.
;

rmcode	segment byte public use16
; We need to mark the first byte in rmcode to figure the real segment value
	public   rmcode_firstbyte
rmcode_firstbyte	label byte

rmcode	ends
rmdata	segment dword public use16
rmdata	ends
pmdata	segment dword public use32
pmdata	ends
pmcode	segment byte public use32
pmcode	ends
dgroup	group	rmdata,pmdata



rmcode segment		; _UDIASMDisconnect will be in rmcode segment

ENDIF


	INCLUDE udidos.ah		; TermStruct Definitions from DFE

_UDIASMDisconnect    LABEL   FAR
;
; Save off important stuff in structure whose address
; is given as third parameter to this function.
	mov	bx, sp
	les	bx, ss:8[bx]
	mov	es:[bx].sss, ss
	mov	es:[bx].ssp, sp
	mov	es:[bx].ssi, si
	mov	es:[bx].sdi, di
	mov	es:[bx].sbp, bp
	mov	es:[bx].sds, ds

IFNDEF DOS386
	jmp	_UDICDisconnect		; real mode, just jump to the C routine
ELSE
	jmp	rm_UDIDisconnect	; DOS386, jump to the real mode stub
ENDIF

_UDIASMConnect    LABEL   FAR
;
; Save off important stuff in structure whose address
; is given as third parameter to this function.
	mov	bx, sp
	les	bx, ss:12[bx]
	mov	es:[bx].sss, ss
	mov	es:[bx].ssp, sp
	mov	es:[bx].ssi, si
	mov	es:[bx].sdi, di
	mov	es:[bx].sbp, bp
	mov	es:[bx].sds, ds
IFNDEF DOS386
	jmp	_UDICConnect		; real mode, just jump to the C routine
ELSE
	jmp	rm_UDIConnect		; DOS386, jump to the real mode stub
ENDIF

IFDEF DOS386
	; Note: the rest of this file is just DOS386 support

rmcode ends

;
; Data that needs to be accessed in both real
; mode and protected mode
;
rmdata	segment

	public code_selector, data_selector, call_prot
	public	segregblock
	; these get filled in by protected mode startup code
code_selector  DW ?
data_selector  DW ?
call_prot	DD ?
segregblock	DW ?	; ds value	; seg reg block filled in at startup time
		DW ?	; es value
		DW ?	; fs value
		DW ?	; gs value

	public TIPName
TIPName		DB  256 DUP(?)

	public TIPVecRec
TIPVecRec	DB  0,0,0,0	; will get filled in by main
	 	DD  0		; next ptr
		DD  0		; prev ptr
		DD  TIPName	; exeName
				; the other entries get added by the udi_table macro below
TIPVecRecEnd	LABEL BYTE

rmdata	ends




;
; Data that is only accessed in prot mode
;
	extrn  conventional_memory:DWORD	; set up by dx_map_physical
	extrn  stack_table: DWORD		; set up by C-level code.

pmdata   segment
dos386glue_table LABEL DWORD	; so we can reference it later in the udi_table macro
				; the entries get added by the udi_table macro below
pmdata   ends



;; The udi_table macro does three things
;;	1) generates real mode entry point for each UDI function
;;	   This code just sets an index in bl and jumps to rm_common
;;	2) adds an entry into the TIPVecRec to point to the above real mode entry point
;;	3) adds an entry into the dos386glue_table table which is used by the prot.mode stub
;;	   to call the actual C glue routine.
 
udi_table	MACRO   UDIProcName,val
rmcode segment
	public	rm_&UDIProcName
rm_&UDIProcName LABEL NEAR
	mov	bl, val		;; bl will indicate which UDI Proc was called
	jmp	short rm_common
rmcode  ends

rmdata segment
	ORG	TIPVecRec + 16 + 4*val	;; Entry in TIPVecRec (+16 for first 4 fields)
	IF	val EQ 0
	DD	_UDIASMConnect		;; special case for Connect
	ELSE
	IF	val EQ 1
	DD	_UDIASMDisconnect	;; special case for Disconnect
	ELSE
	DD	rm_&UDIProcName		;; normal entry is rm_ stub
	ENDIF
	ENDIF
rmdata ends

pmdata segment
	EXTRN	d386_&UDIProcName:NEAR
	ORG	dos386glue_table + 4*val	;; this builds the jump table that pmstub uses
	DD	d386_&UDIProcName
pmdata ends

	ENDM

	udi_table   UDIConnect,0
	udi_table   UDIDisconnect,1
        udi_table   UDISetCurrentConnection,2
        udi_table   UDICapabilities,3
        udi_table   UDIGetErrorMsg,4
        udi_table   UDIGetTargetConfig,5
        udi_table   UDICreateProcess,6
        udi_table   UDISetCurrentProcess,7
        udi_table   UDIDestroyProcess,8
        udi_table   UDIInitializeProcess,9
        udi_table   UDIRead,10
        udi_table   UDIWrite,11
        udi_table   UDICopy,12
        udi_table   UDIExecute,13
        udi_table   UDIStep,14
        udi_table   UDIStop,15
        udi_table   UDIWait,16
        udi_table   UDISetBreakpoint,17
        udi_table   UDIQueryBreakpoint,18
        udi_table   UDIClearBreakpoint,19
        udi_table   UDIGetStdout,20
        udi_table   UDIGetStderr,21
        udi_table   UDIPutStdin,22
        udi_table   UDIStdinMode,23
        udi_table   UDIPutTrans,24
        udi_table   UDIGetTrans,25
        udi_table   UDITransMode,26


rmcode  segment
	ASSUME	nothing, CS:rmcode	; all we know is that CS=rmcode


	; Common real mode stub code
	; bl is an index indicating which UDI function was called
	; we need to switch to protected mode (ebx will be passed thru unchanged)
rm_common  PROC FAR		; UDI always called as far (real mode)
	push	ds		; save ds
	push	bp		; and save bp
	push	si		; and si, di
	push	di
				; note: if anything else gets pushed here, you must
				; change the MSCPARAMS macro in dos386c.c
	push	cs
	pop	ds		; set ds = cs
	ASSUME ds:dgroup
	; to switch to protected mode, we push a dword ptr to a block which
	; contains the protected mode segment registers to use
	; and we push the 48-bit protected address of pmstub
	; then we call the call_prot routine which was returned by dx_rmlink_get

	push	cs		; segment of seg reg block
	lea	ax, segregblock
	push	ax		; offset of seg reg block
   	push	code_selector
	lea	eax, pmstub
	push	eax
	call	call_prot
	add	sp, 10		; unpop all things we pushed
				; ax return code from prot mode passed thru
	
	pop	di		; unpop di,si saved earlier
	pop	si
	pop	bp		; unpop the BP we saved earlier
	pop	ds		; unpop the DS we saved earlier
	ret 			; will do a FAR (real mode) ret to DFE

rm_common  ENDP

rmcode  ends

	
	

pmcode segment

pmstub proc far
	; at this point ss:sp -> far return (DF) back to dos-extender
	;		   sp+6	 a word of 0
	;		  sp+8	 the pushed DS (of rmstub)
	;		  sp+10  parameters
	; we pop the far return and save it away
	; then we call the real application procedure (the params still on stack)
	ASSUME CS:pmcode, DS:dgroup
	; first let's switch ss:esp so that ss = ds
	; (it will still point to the same physical memory location)
	; (we'll save the old ss:esp on the stack in case they're needed)
	mov	dx, ss
	mov 	ecx, esp
	; at this point, ecx contains the physical 32-bit address of sp
	; (selector 60h's offset mapped directly to 1meg physical memory)

	mov	eax, conventional_memory
	cmp	eax, 0		; if conventional memory not mapped take other path
	je	short conv_mem_unmapped
 
  		; This is the code that is not DPMI compatible
		; we add the ofst of beggining of conventional memory to esp
		; to make it SS_DATA relative and then use data_selector as the SS
		; thus no stack switch is necessary, we just remap the old stack
	add	eax, ecx	; this adds esp to conventional_memory
	mov	ss, data_selector
	mov	esp, eax
	jmp	short got_ss_sp
		; now ss:esp points to same place as before but using different segment


conv_mem_unmapped:
   		; This code is DPMI compatible
   		; we actually switch to a new stack that is in the TIPs DS
   		; the number and size of these stacks was allocated at startup time.
	lea	eax, stack_table
chk_stack:
	cmp	dword ptr [eax], 0
	je	short no_stack	; stack pointer of 0 means end of table
	mov	esi, dword ptr [eax]	; get stack pointer
	cmp	dword ptr [esi+4], 0	; is it marked free?
	je	short take_stack
	add	eax, 4		; to next stack entry in table
	jmp	chk_stack

no_stack:
	mov	eax, 25		; IPC Limitation error
	ret

take_stack:
	mov	dword ptr [esi+4], 0ffffffffh	; mark stack in use
	mov	ss, data_selector
	mov	esp, esi	; get stack pointer from table
				; now we have ss = ds, so we can go to C level.

got_ss_sp:
	; push the old ss:esp on the stack
	push	edx		; old ss (need to push as full 32-bit for C convention)
	push	ecx		; old esp

	; bl still contains the UDIProcedure Index (which was set up by the rm stub)
	; use this to get to the correct dos386glue_table routine 
	xor	bh, bh
	shl	bx,1		; *4 for indexing into DD array
	shl	bx,1
	call	dos386glue_table[bx]
	; on return, we just need to restore the old ss:esp
	pop	ecx
	pop	edx

	cmp	conventional_memory, 0	; if we had switched stacks
	jne	short no_stack_clear
	mov	dword ptr [esp+4], 0	; clear the stack in use indicator

no_stack_clear:
				; this code is identical whether we switched stacks or not
				; we just restore the old ss:sp and return
	mov	ss, dx
	mov	esp, ecx
				; the stack should look as it did when we entered
				; so we just do a far ret
	ret
pmstub endp


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


	public	_exp_return
	extrn	_top:dword

_exp_return PROC FAR
	; set the stack pointer back to its initial state as left by dfe
	; (leave the ss unchanged)
	; then do a far ret which will get us back to the dfe.
	; (which will then restore its own stack).
	mov	ebp, esp
	mov	eax, [ebp+4]	; errcode
	mov	ecx, _top
	add	ecx, 2
	mov	esp, ecx
	ret
_exp_return ENDP

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



ENDIF	; end of DOS386 conditional code

	END
