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
* So that all may benefit from your experience, please report  any  problems
* or  suggestions about this software to the 29K Technical Support Center at
* 800-29-29-AMD (800-292-9263) in the USA, or 0800-89-1131  in  the  UK,  or
* 0031-11-1129 in Japan, toll free.  The direct dial number is 512-462-4118.
*
* Advanced Micro Devices, Inc.
* 29K Support Products
* Mail Stop 573
* 5900 E. Ben White Blvd.
* Austin, TX 78741
* 800-292-9263
*****************************************************************************
*/
static char udisoc_h[]="@(#)udisoc.h	2.6  Daniel Mann";
static char udisoc_h_AMD[]="@(#)udisoc.h	2.4, AMD";
/*
*       This module defines constants used in the UDI IPC modules
********************************************************************** HISTORY
*/
#define LOCAL static
#define	company_c	1		/* AMD Company id */
#define	product_c 	1		/* socket IPC id */

/* Enumerate the UDI procedure services 
*/
#define	UDIConnect_c			0
#define	UDIDisconnect_c			1
#define	UDISetCurrentConnection_c	2
#define	UDICapabilities_c		3
#define	UDIEnumerateTIPs_c		4
#define	UDIGetErrorMsg_c		5
#define	UDIGetTargetConfig_c		6
#define	UDICreateProcess_c		7
#define	UDISetCurrentProcess_c		8
#define	UDIDestroyProcess_c		9
#define	UDIInitializeProcess_c		10
#define	UDIRead_c			11
#define	UDIWrite_c			12
#define	UDICopy_c			13
#define	UDIExecute_c			14
#define	UDIStep_c			15
#define	UDIStop_c			16
#define	UDIWait_c			17
#define	UDISetBreakpoint_c		18
#define	UDIQueryBreakpoint_c		19
#define	UDIClearBreakpoint_c		20
#define	UDIGetStdout_c			21
#define	UDIGetStderr_c			22
#define	UDIPutStdin_c			23
#define	UDIStdinMode_c			24
#define	UDIPutTrans_c			25
#define	UDIGetTrans_c			26
#define	UDITransMode_c			27
#define	UDITest_c			28
#define	UDIKill_c			29

#define	udr_UDIInt8(udrs, obj)  udr_work(udrs, obj, 1)
#define	udr_UDIInt16(udrs, obj) udr_work(udrs, obj, 2)
#define	udr_UDIInt32(udrs, obj) udr_work(udrs, obj, 4)
#define	udr_UDIInt(udrs, obj)   udr_work(udrs, obj, 4)

#define	udr_UDIUInt8(udrs, obj)  udr_work(udrs, obj, 1)
#define	udr_UDIUInt16(udrs, obj) udr_work(udrs, obj, 2)
#define	udr_UDIUInt32(udrs, obj) udr_work(udrs, obj, 4)
#define	udr_UDIUInt(udrs, obj)   udr_work(udrs, obj, 4)

#define	udr_UDIBool(udrs, obj)   udr_UDIInt32(udrs, obj)
#define	udr_UDICount(udrs, obj)  udr_UDIInt32(udrs, obj)
#define	udr_UDISize(udrs, obj)   udr_UDIUInt32(udrs, obj)
#define	udr_CPUSpace(udrs, obj)  udr_UDIInt32(udrs, obj)
#define	udr_CPUOffset(udrs, obj) udr_UDIUInt32(udrs, obj)
#define	udr_CPUSizeT(udrs, obj)  udr_UDIUInt32(udrs, obj)
#define	udr_UDIBreakId(udrs,obj) udr_UDIUInt(udrs, obj)
#define	udr_UDISizeT(udrs, obj)  udr_UDIUInt(udrs, obj)
#define	udr_UDIMode(udrs, obj)   udr_UDIUInt(udrs, obj)

#define	udr_UDIHostMemPtr(udrs, obj) udr_UDIUInt32(udrs, obj)
#define	udr_UDIVoidPtr(udrs, obj)   udr_UDIUInt32(udrs, obj)
#define	udr_UDIPId(udrs, obj)       udr_UDIUInt(udrs, obj)
#define	udr_UDISessionId(udrs, obj) udr_UDIInt32(udrs, obj)
#define	udr_UDIError(udrs, obj)     udr_UDIInt32(udrs, obj)
#define	udr_UDIStepType(udrs, obj)  udr_UDIInt32(udrs, obj)
#define	udr_UDIBreakType(udrs, obj) udr_UDIInt32(udrs, obj)

 
#define	UDR_ENCODE 1
#define	UDR_DECODE 2

typedef struct	UDR_str
{
    int		udr_op;			/* UDR operation */
    int		previous_op;
    int		sd;
    int		bufsize;
    char*	buff;
    char*	getbytes;
    char*	putbytes;
    char*	putend;
    int		domain;
    char*	soc_name;
} UDR;

/******************************************* Declare UDR suport functions */
int udr_create UDIParams((
  UDR*	udrs,
  int	sd,
  int	size
  ));

int udr_free UDIParams((
  UDR*	udrs,
  ));

int udr_signal UDIParams((
  UDR*	udrs,
  ));

int udr_sendnow UDIParams((
  UDR*	udrs
  ));

int udr_work UDIParams((
  UDR*	udrs,
  void*	object_p,
  int	size
  ));

int udr_UDIResource UDIParams((
  UDR*	udrs,
  UDIResource*	object_p
  ));

int udr_UDIRange UDIParams((
  UDR*	udrs,
  UDIRange*	object_p
  ));

int udr_UDIMemoryRange UDIParams((
  UDR*	udrs,
  UDIMemoryRange*	object_p
  ));

int udr_UDIMemoryRange UDIParams((
  UDR*	udrs,
  UDIMemoryRange* object_p
  ));

int udr_int UDIParams((
  UDR*	udrs,
  int*	int_p
  ));

int udr_bytes UDIParams((
  UDR*	udrs,
  char*	ptr,
  int	len
  ));

char* udr_inline UDIParams((
  UDR*	udrs,
  int	size
  ));

char*	udr_getpos UDIParams((
  UDR*	udrs
  ));
int	udr_setpos UDIParams((
  UDR*	udrs,
  char*	pos
  ));

int	udr_readnow UDIParams((
  UDR*	udrs,
  int	size
  ));

int udr_align UDIParams((
  UDR*	udrs,
  int	size,
  ));
