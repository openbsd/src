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
 *       $Id: udidos.h,v 1.2 1996/11/23 04:11:23 niklas Exp $
 *       $Id: @(#)udidos.h	2.8, AMD
 */

/* Modified: M.Typaldos 11/92 - Added '386 specific code (just changed 
 *                   far to _FAR really).
 */

#ifdef DOS386
#include <pharlap.h>
#include <pltypes.h>
#define _FAR 
typedef REALPTR  RealUDIVecRecPtr;
#else
#define _FAR far
typedef  struct UDIVecRec far *RealUDIVecRecPtr;
#endif

/* Structure used to deal with shutting down of TIPs. This
   structure is primarily used by assembly language code.
 */
typedef UDIStruct DOSTerm {
    void (far *TermFunc)(void);
    UDIUInt16 sds;
    UDIUInt16 sss;
    UDIUInt16 ssi;
    UDIUInt16 sdi;
    UDIUInt16 ssp;
    UDIUInt16 retval;
    UDIUInt16 sbp;
} DOSTerm;



union rec {
    unsigned char c[4];
    long l;
    };


#define UDIDOSTIPRecognizer	"\xcfudi"
#define InitRecognizer( x )	memcpy( (x), UDIDOSTIPRecognizer, 4 )

struct UDIVecRec {
    union rec recognizer;	/* Our "who we are" matching string */
    RealUDIVecRecPtr      Next;	/* Pointer to next TIP */
    RealUDIVecRecPtr	  Prev;	/* Pointer to previous TIP */
    char _FAR *exeName;		/* Name of the executable we were loaded as */
    UDIError (_FAR *UDIConnect) UDIParams((
      char		far *Configuration,	/* In */
      UDISessionId	far *Session,		/* Out */
      DOSTerm		far *TermStruct 	/* In - not seen in UDIP */
      ));
    UDIError (_FAR *UDIDisconnect) UDIParams((
      UDISessionId	Session,		/* In */
      UDIBool		Terminate,		/* In */
      DOSTerm		far *TermStruct 	/* In - not seen in UDIP */
      ));
    UDIError (_FAR *UDISetCurrentConnection) UDIParams((
      UDISessionId	Session			/* In */
      ));
    UDIError (_FAR *UDICapabilities) UDIParams((
      UDIUInt32		far *TIPId,		/* Out */
      UDIUInt32		far *TargetId,		/* Out */
      UDIUInt32		DFEId,			/* In */
      UDIUInt32		DFE,			/* In */
      UDIUInt32		far *TIP,		/* Out */
      UDIUInt32		far *DFEIPCId,		/* Out */
      UDIUInt32		far *TIPIPCId,		/* Out */
      char		far *TIPString		/* Out */
      ));
    UDIError (_FAR *UDIGetErrorMsg) UDIParams((
      UDIError		ErrorCode,		/* In */
      UDISizeT		MsgSize,		/* In */
      char		far *Msg,		/* Out */
      UDISizeT		far *CountDone		/* Out */
      ));
    UDIError (_FAR *UDIGetTargetConfig) UDIParams((
      UDIMemoryRange	far KnownMemory[],	/* Out */
      UDIInt		far *NumberOfRanges,	/* In/Out */
      UDIUInt32		far ChipVersions[],	/* Out */
      UDIInt		far *NumberOfChips	/* In/Out */
      ));
    UDIError (_FAR *UDICreateProcess) UDIParams((
      UDIPId		far *PId		/* Out */
      ));
    UDIError (_FAR *UDISetCurrentProcess) UDIParams((
      UDIPId		PId			/* In */
      ));
    UDIError (_FAR *UDIDestroyProcess) UDIParams((
      UDIPId		PId			/* In */
      ));
    UDIError (_FAR *UDIInitializeProcess) UDIParams((
      UDIMemoryRange	far ProcessMemory[],	/* In */
      UDIInt		NumberOfRanges,		/* In */
      UDIResource	EntryPoint,		/* In */
      CPUSizeT		far StackSizes[],	/* In */
      UDIInt		NumberOfStacks,		/* In */
      char		far *ArgString		/* In */
      ));
    UDIError (_FAR *UDIRead) UDIParams((
      UDIResource	From,			/* In */
      UDIHostMemPtr	To,			/* Out */
      UDICount		Count,			/* In */
      UDISizeT		Size,			/* In */
      UDICount		far *CountDone,		/* Out */
      UDIBool		HostEndian		/* In */
      ));
    UDIError (_FAR *UDIWrite) UDIParams((
      UDIHostMemPtr	From,			/* In */
      UDIResource	To,			/* In */
      UDICount		Count,			/* In */
      UDISizeT		Size,			/* In */
      UDICount		far *CountDone,		/* Out */
      UDIBool		HostEndian		/* In */
      ));
    UDIError (_FAR *UDICopy) UDIParams((
      UDIResource	From,			/* In */
      UDIResource	To,			/* In */
      UDICount		Count,			/* In */
      UDISizeT		Size,			/* In */
      UDICount		far *CountDone,		/* Out */
      UDIBool		Direction		/* In */
      ));
    UDIError (_FAR *UDIExecute) UDIParams((
      void
      ));
    UDIError (_FAR *UDIStep) UDIParams((
      UDIUInt32		Steps,			/* In */
      UDIStepType	StepType,		/* In */
      UDIRange		Range			/* In */
      ));
    UDIVoid (_FAR *UDIStop) UDIParams((
      void
      ));
    UDIError (_FAR *UDIWait) UDIParams((
      UDIInt32		MaxTime,		/* In */
      UDIPId		far *PId,		/* Out */
      UDIUInt32		far *StopReason		/* Out */
      ));
    UDIError (_FAR *UDISetBreakpoint) UDIParams((
      UDIResource	Addr,			/* In */
      UDIInt32		PassCount,		/* In */
      UDIBreakType	Type,			/* In */
      UDIBreakId	far *BreakId		/* Out */
      ));
    UDIError (_FAR *UDIQueryBreakpoint) UDIParams((
      UDIBreakId	BreakId,		/* In */
      UDIResource	far *Addr,		/* Out */
      UDIInt32		far *PassCount,		/* Out */
      UDIBreakType	far *Type,		/* Out */
      UDIInt32		far *CurrentCount	/* Out */
      ));
    UDIError (_FAR *UDIClearBreakpoint) UDIParams((
      unsigned int	BreakId			/* In */
      ));
    UDIError (_FAR *UDIGetStdout) UDIParams((
      UDIHostMemPtr	Buf,			/* Out */
      UDISizeT		BufSize,		/* In */
      UDISizeT		far *CountDone		/* Out */
      ));
    UDIError (_FAR *UDIGetStderr) UDIParams((
      UDIHostMemPtr	Buf,			/* Out */
      UDISizeT		BufSize,		/* In */
      UDISizeT		far *CountDone		/* Out */
      ));
    UDIError (_FAR *UDIPutStdin) UDIParams((
      UDIHostMemPtr	Buf,			/* In */
      UDISizeT		Count,			/* In */
      UDISizeT		far *CountDone		/* Out */
      ));
    UDIError (_FAR *UDIStdinMode) UDIParams((
      UDIMode		far *Mode		/* Out */
      ));
    UDIError (_FAR *UDIPutTrans) UDIParams((
      UDIHostMemPtr	Buf,			/* In */
      UDISizeT		Count,			/* In */
      UDISizeT		far *CountDone		/* Out */
      ));
    UDIError (_FAR *UDIGetTrans) UDIParams((
      UDIHostMemPtr	Buf,			/* Out */
      UDISizeT		BufSize,		/* In */
      UDISizeT		far *CountDone		/* Out */
      ));
    UDIError (_FAR *UDITransMode) UDIParams((
      UDIMode		far *Mode		/* Out */
      ));
    };


