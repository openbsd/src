/******************************************************************************
 * Copyright 1991 Advanced Micro Devices, Inc.
 *
 * This software is the property of Advanced Micro Devices, Inc  (AMD)  which
 * specifically  grants the user the right to modify, use and distribute this
 * software provided this notice is not removed or altered.  All other rights
 * are reserved by AMD.
 *
 * AMD MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARD TO THIS
 * SOFTWARE.  IN NO EVENT SHALL AMD BE LIABLE FOR INCIDENTAL OR CONSEQUENTIAL
 * DAMAGES IN CONNECTION WITH OR ARISING FROM THE FURNISHING, PERFORMANCE, OR
 * USE OF THIS SOFTWARE.
 *
 * Comments about this software should be directed to udi@amd.com. If access
 * to electronic mail isn't available, send mail to:
 *
 * Advanced Micro Devices, Inc.
 * 29K Support Products
 * Mail Stop 573
 * 5900 E. Ben White Blvd.
 * Austin, TX 78741
 *****************************************************************************
 *       $Id: dos386c.h,v 1.2 1996/11/23 04:11:15 niklas Exp $
 *       $Id: @(#)dos386c.h	1.6, AMD
 */

#ifndef _DOS386_H
#define _DOS386_H

#ifdef WATC 
#define NULL 0
#endif /* WATC */


#ifdef _DOS386C_C
#define EXTERN 
#define STATIC static
#else
#define EXTERN extern
#define STATIC
#endif

#define FAR

EXTERN char    *conventional_memory;	/* pointer to first byte of conventinal memory     */

EXTERN USHORT	our_tsr_psp;
EXTERN USHORT	dos_ext_psp;
/* EXTERN STATIC	struct UDIVecRec FAR * FAR * UDIVecP; */
extern	REALPTR	call_prot;
extern	USHORT	code_selector;
extern	USHORT	data_selector;
extern	USHORT	rmcode_firstbyte;


/*
 * Used to copy real mode code and data into conventional memory (udip2dos)
 * and to construct real mode pointers and protected mode far (48 bit) 
 * pointers.
 */

EXTERN	USHORT	rmem_adr;
EXTERN	REALPTR	real_base;
EXTERN	FARPTR	prot_base;


/*=============== Utilities for converting pointers from prot to real and vice versa========*/

	/* LINEARIZE converts a segment:ofst pointer into a linear addr between 0 and 1meg */
#define LINEARIZE(rp) ((RP_SEG(rp)<<4) + RP_OFF(rp))



FARPTR REAL_TO_PROT(REALPTR rp);


/* in DOS386, TIPName, TIPVecRec defined in dos386a.asm in rmdata */
extern char TIPName[];
extern struct UDIVecRec TIPVecRec;


/* Function Prototypes */

REALPTR PROT_TO_REAL(FARPTR p);
REALPTR _far * FindIntVect();
FARPTR NEARPTR_TO_FARPTR(void *ptr);



/* Prototypes for DOS386 UDI interface functions. */


UDIError UDIPConnect(REALPTR UDIConnectAddr, 
	char *Configuration, 
	UDISessionId *Session,
	DOSTerm _far * TermStruct
	);



UDIError UDIPCapabilities(
	REALPTR		UDICapabilitiesAddr,
	UDIUInt32	*TIPId,			/* Out */
	UDIUInt32	*TargetId,		/* Out */
	UDIUInt32	DFEId,			/* In  */
	UDIUInt32	DFE,			/* In  */
	UDIUInt32	*TIP,			/* Out */
	UDIUInt32	*DFEIPCId,		/* Out */
	UDIUInt32	*TIPIPCId,		/* Out */
	char		*TIPString		/* Out */
  	);

UDIError UDIPGetErrorMsg(
	REALPTR		UDIGetErrorMessageAddr,
	UDIError	ErrorCode,		/* In  */
	UDISizeT	MsgSize,		/* In  */
	char		*Msg,			/* Out */
	UDISizeT	*CountDone		/* Out */
	);

UDIError UDIPSetCurrentConnection(
	REALPTR		UDISetCurrentConnectionAddr,
	UDISessionId	Session			/* In  */
	);

UDIError UDIPDisconnect(
	REALPTR		UDIDisconnectAddr,
	UDISessionId	Session,		/* In  */
	UDIBool		Terminate,		/* In  */
	DOSTerm _far * TermStruct	/* In  */
	);

UDIError UDIPGetTargetConfig(
	REALPTR		UDIGetTargetConfigAddr,
	UDIMemoryRange	KnownMemory[],		/* Out */
	UDIInt		*NumberOfRanges,	/* In/Out */
	UDIUInt32	ChipVersions[],		/* Out */
	UDIInt		*NumberofChips		/* In/Out */
	);

UDIError UDIPCreateProcess(
	REALPTR		UDICreateProcessAddr,
	UDIPId		*PId			/* Out */
	);

UDIError UDIPSetCurrentProcess(
	REALPTR		UDISetCurrentProcessAddr,
	UDIPId		PId			/* In  */
	);

UDIError UDIPInitializeProcess(
	REALPTR		UDIInitializeProcessAddr,
	UDIMemoryRange	ProcessMemory[],	/* In  */
	UDIInt		NumberOfRanges,		/* In  */
	UDIResource	EntryPoint,		/* In  */
	CPUSizeT	*StackSizes,		/* In  */
	UDIInt		NumberOfStacks,		/* In  */
	char		*ArgString		/* In  */
	);

UDIError UDIPDestroyProcess(
	REALPTR		UDIDestroyProcessAddr,
	UDIPId		PId			/* In  */
	);

UDIError UDIPRead(
	REALPTR		UDIReadAddr,
	UDIResource	From,			/* In  */
	UDIHostMemPtr	To,			/* Out */
	UDICount	Count,			/* In  */
	UDISizeT	Size,			/* In  */
	UDICount	*CountDone,		/* Out */
	UDIBool		HostEndian		/* In  */
	);

UDIError UDIPWrite(
	REALPTR		UDIWriteAddr,
	UDIHostMemPtr	From,			/* In  */
	UDIResource	To,			/* In  */
	UDICount	Count,			/* In  */
	UDISizeT	Size,			/* In  */
	UDICount	*CountDone,		/* Out */
	UDIBool		HostEndian		/* In  */
	);

UDIError UDIPCopy(
	REALPTR		UDICopyAddr,
	UDIResource	From,			/* In  */
	UDIResource	To,			/* In  */
	UDICount	Count,			/* In  */
	UDISizeT	Size,			/* In  */
	UDICount	*CountDone,		/* Out */
	UDIBool		Direction		/* In  */
	);

UDIError UDIPExecute(
	REALPTR		UDIExecuteAddr
	);

UDIError UDIPStep(
	REALPTR		UDIStepAddr,
	UDIUInt32	Steps,			/* In  */
	UDIStepType	StepType,		/* In  */
	UDIRange	Range			/* In  */
	);

UDIError UDIPStop(
	REALPTR		UDIStopAddr
	);

UDIError UDIPWait(
	REALPTR		UDIWaitAddr,
	UDIInt32	MaxTime,		/* In  */
	UDIPId		*PId,			/* Out */
	UDIUInt32	*StopReason		/* Out */
	);

UDIError UDIPSetBreakpoint(
	REALPTR		UDISetBreakpointAddr,
	UDIResource	Addr,	  		/* In  */
	UDIInt32	PassCount,		/* In  */
	UDIBreakType	Type,			/* In  */
	UDIBreakId	*BreakId		/* Out */
	);

UDIError UDIPQueryBreakpoint(
	REALPTR		UDIQueryBreakpointAddr,
	UDIBreakId	BreakId,		/* In  */
	UDIResource	*Addr,			/* Out */
	UDIInt32	*PassCount,		/* Out */
	UDIBreakType	*Type,			/* Out */
	UDIInt32	*CurrentCount		/* Out */
	);

UDIError UDIPClearBreakpoint(
	REALPTR		UDIClearBreakpointAddr,
	UDIBreakId	BreakId			/* In  */
	);


UDIError UDIPGetStdout(
	REALPTR		UDIGetStdoutAddr,
	UDIHostMemPtr	Buf,			/* Out */
	UDISizeT	BufSize,		/* In  */
	UDISizeT	*CountDone		/* Out */
	);

UDIError UDIPGetStderr(
	REALPTR		UDIGetStderrAddr,
	UDIHostMemPtr	Buf,			/* Out */
	UDISizeT	BufSize,		/* In  */
	UDISizeT	*CountDone		/* Out */
	);

UDIError UDIPPutStdin(
	REALPTR		UDIPutStdinAddr,
	UDIHostMemPtr	Buf,			/* In  */
	UDISizeT	Count,			/* In  */
	UDISizeT	*CountDone		/* Out */
	);

UDIError UDIPStdinMode(
	REALPTR		UDIStdinModeAddr,
	UDIMode		*Mode			/* Out */
	);

UDIError UDIPPutTrans(
	REALPTR		UDIPutTransAddr,
	UDIHostMemPtr	Buf,			/* In  */
	UDISizeT	Count,			/* In  */
	UDISizeT	*CountDone		/* Out */
	);

UDIError UDIPGetTrans(
	REALPTR		UDIGetTransAddr,
	UDIHostMemPtr	Buf,			/* Out */
	UDISizeT	BufSize,		/* In  */
	UDISizeT	*CountDone		/* Out */
	);

UDIError UDIPTransMode(
	REALPTR		UDITranModesAddr,
	UDIMode		*Mode			/* Out */
	);


#if __HIGHC__
/* make _fmemmove work for highc */

static void _fmemmove(void _far *dst, void _far *src, int count)
{
  /* Metaware hc386 call */
  _movedata(FP_SEL(src), /* from segment:ofs */
	    FP_OFF(src),
	    FP_SEL(dst), /* to segment:ofst */
	    FP_OFF(dst),
	    count);
}
#endif
	


#endif  /* _DOS386_H */



