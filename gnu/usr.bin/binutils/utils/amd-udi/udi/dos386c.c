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
 *       $Id: dos386c.c,v 1.2 1996/11/23 04:11:14 niklas Exp $
 *       $Id: @(#)dos386c.c	1.9, AMD
 */

/* DOS386 specific constructs and functions.  These assume the use of
 * the 386|DOS-Extender from Pharlap.
 *
 * This file contains functions which serve as an intermediate between
 * protected mode UDI function calls made by the DFE and the possibly
 * real-mode function called in the TIP.  These functions assume that
 * the interface between the TIP and the DFE is real-mode and that the
 * DFE is protected mode, flat model (32 bit linear addresses).
 *
 * Note:
 * This code allocates a section of real-mode memory using
 * a call to _dx_real_alloc.  Protected mode far pointers (48-bit)
 * must be used to access this memory area.
 *
 * For every UDI function named UDIx..x, there is a function in this
 * module named UDIPx..x, where the "P" indicates that this is a protected
 * mode interface function.  A protected mode DFE needs to call the
 * UDIPx..x equivalent of the call to the TIP UDI function.  The call
 * parameters are the same as the UDI call except that the address of the
 * TIP UDI function is always the first parameter to the UDIPx..x 
 * function.
 *
 * Each function follows the format outlined below:
 *
 * UDIError UDIPx..x(REALPTR function_address [, UDI function parameters])
 * {
 * 	Allocate pointers into the conventional memory area used for
 *		parameter passing (PPM) for all parameters which are
 *		passed-by-reference.  Each of these pointers is given
 *		the same name as a parameter with "Ptr" appended to it.
 *
 *	Create a packed structure for parameter passing if there is more
 *		than one parameter to the function.  Each member of this
 *		structure (always named params) has the same name as the
 *		corresponding parameter to the UDIP... call.
 *
 *	Set the parameter pointers to the appropriate offsets in the PPM.
 *		The protected mode pointer to the first parameter is always
 *		rm_address (the real mode equivalent to this pointer is
 *		rm_buffer_addr).
 *
 *	Copy the data from protected mode (possibly extended) memory to
 *		the location indicated by the correct pointer 
 *
 *	Copy the passed-by-value parameters directly into the params
 *		structure.  Convert the protected mode version of the
 *		pointers into the PPM area to real-mode pointers and
 *		assign them to their corresponding params data member.
 *
 *	Call the real-mode function, pushing the params structure onto 
 *		the stack.  Generally this involves the use of the macro
 *		REALCALL, however functions with no parameters call 
 *		_dx_call_real explicitly.  The size of the params structure
 *		is determined by the macro WORDSIZE.
 *
 *	Check for an error returned from _dx_call_real.  If there is one
 *		report it and die (how uncouth!).
 *
 *	Copy all "out" parameters into their local (non-conventional 
 *		memory eqivalents.  In functions with "count" parameters,
 *		make sure that the count value makes sense and only 
 *		copy as much as is allowed by the buffer size.
 *	
 *	The return value of the UDI TIP function is in the ax register of
 *		the regs structure, return this as the value of the UDIP
 *		function.
 * }
 *
 *
 * UDIPRead, UDIPWrite, UDIPPutStdOut, UDIPGetStdOut, 
 * UDIPGetStderr, UDIPPutTrans and UDIPGetTrans differ from the other
 * UDIP functions in that they allow looping within the UDIP layer
 * call to the TIP.  This looping is done so that the size of the
 * real mode buffer area does not limit the size of the data transfer
 * since all data passed by reference *must* be copied into the real
 * mode buffer area and the TIP can only return as much information
 * as can fit in this real mode buffer (unlike the situation for a 
 * real mode DFE where the DFE and the TIP write to the same memory).
 *
 * The calls all use the same logic, outlined below:
 *
 *
 * Set the CurrentCount equal to the total number of items to 
 * be transfered (CurrentCount = *Count).
 *
 * Set the total number of items transfered so far to zero (TotalDone = 0)
 *
 * Set up moving pointers into the From and To transfer areas to keep
 * track of where the current transfer should be read and/or written.
 * (CurrentTo = To; CurrentFrom = From)
 *
 * do 
 *    Set a pointer to the end of the memory that would be required
 *    to do the complete transfer.
 *
 *    If the pointer is outside of the buffer area (determined by
 *    call the BufferSizeCheck), then we need to adjust the
 *    size of the requested transfer.
 *
 *       Set the pointer to the last item to the last valid location
 *       in the real mode buffer area.
 *
 *       Set the value of CurrentCount to the maximum number of data
 *       items that can be transfered, based on the Size parameter.
 *
 *    Call the TIP function with CurrentCount instead of *Count,
 *    CurrentTo instead of To and CurrentFrom instead of From.
 *
 *    Set the CurrentDone equal to the CountDone returned by the
 *    function call.
 *
 *    Update the pointers into the From and To transfer areas to the
 *    end of the just completed transfer (CurrentFrom += CurrentDone
 *    * Size, CurrentTo += CurrentDone * Size)
 *
 *    Increment the TotalDone by the number of items transfered in
 *    the last call (TotalDone += CurrentDone)
 *
 * while transfer is not complete (TotalDone < *Count)
 *
 */



#define _DOS386C_C

#include <dos.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <udiproc.h>

#include <pharlap.h>
#include <udidos.h>
#include <stdarg.h>


#include <dos386c.h>


#define REAL_BUFFER_SIZE 0x1000
#define PRINT_ON  1
#define PRINT_OFF 0

#define WORDSIZE(param) ((sizeof(param)/2) + (sizeof(param) %2))

#define REALCALL(x,y) _dx_call_real(x, &real_regs, WORDSIZE(y), y)

#define FUNC_VAL ((UDIError) real_regs.eax)

#define SIZE_ERROR(Done, Size, function_name) printf("Return size (%d) > Buffer size (%d) in function %s\n",Done,Size,function_name)

/*
 * 4/93 MDT
 * The following defines are used to allow UDI 1.2 functions to identify
 * themselves to the compiler and the user when UDI 1.3 is defined.  These
 * are needed to differentiate the 1.2 versions of some functions (UDIPConnect)
 * from their 1.3 versions.
 */
#if defined __STDC__ || defined _MSC_VER
#define XCAT(a,b) a##b
#else
#define XCAT(a,b) a/**/b
#endif

#ifdef UDI13 

#define UDI12FUNCVER _12
#define UDI12FUNCVERSTR " (1.2)"
#define CAT(a,b) XCAT(a,b)

#else /* not UDI 1.3 */

#define UDI12FUNCVER dummy	/* Have to pass two arguments to CAT */ 
#define UDI12FUNCVERSTR ""
#define CAT(a,b) a		/* Don't actually want to concatenate anything */

#endif /* UDI 1.3 */


/* Needed by call to _dx_real_call, but no values need to be set
 * since TIP is compiled assuming it doesn't know anything about the
 * values in the registers.
 */
RMC_BLK  real_regs;	



/* Pointers to use for handling conventional memory buffer area.  This
 * area is used to pass parameters to and from real-mode procedures.
 */
REALPTR	rm_buffer_addr;		/* real-mode pointer to parameter buffer area.     */
ULONG		rm_buffer_size;		/* size of paramter buffer area.                   */

USHORT _far	*rm_address;		/* pointer to paramter buffer area.                */
USHORT _far	*rm_end_address;	/* the last valid address of the parameter buffer  */


int		buffer_allocated=0;	/* used to denote that the status of the buffer    */




REALPTR PROT_TO_REAL(FARPTR p)		/* converts a PROT ptr to a REALPTR */
{
REALPTR  dummyrp;
FARPTR   dummyfp;
int   err;

	/* returns a real mode pointer given a prot mode pointer p */

//	FP_SET(dummyfp,p,data_selector);
	dummyfp = p;
	err = _dx_toreal(dummyfp, 0, &dummyrp);
	if (err) {
	   printf("Fatal Error _dx_toreal (0x%lX)\n", (ULONG) p);
	   exit(err);
	}
	return(dummyrp);
	
} /* PROT_TO_REAL */



FARPTR REAL_TO_PROT(REALPTR rp)  /* converts a REALPTR to a FARPTR */
{
FARPTR	dummyfp;

	FP_SET(dummyfp,LINEARIZE(rp),SS_DOSMEM);
	return (dummyfp); 

} /* REAL_TO_PROT */



FARPTR NEARPTR_TO_FARPTR(void *ptr)
/* Convert a near (32 bit linear) pointer to a far (48 bit) pointer. */
{
	FARPTR dummyfptr;

	FP_SET(dummyfptr, ptr, data_selector);

	return(dummyfptr);

} /* NEARPTR_TO_FARPTR() */


long BufferSizeCheck(FARPTR ptr, char *function_name, int print_message)
/* Check passed ptr to make sure that it points to a valid location in
 * the real-mode parameter passing buffer.  If not, just report an
 * error for now.
 */
{
	if ((long)ptr < (long)rm_address) {
		printf("Invalid address for real mode parameters in function: %s\n",function_name);
		exit(1);
	}
	if ((long)ptr > (long)rm_end_address) {
		if (print_message) {
			printf("Parameters too large for passing to real mode in function: %s\n",function_name);
			printf("Value of ptr - rm_end_address:%ld\n",(long)(ptr - (FARPTR)rm_end_address));
		}
		return (long)((long)ptr - (long)rm_end_address);
	}

	return 0;	/* passed the size check */

} /* BufferSizeCheck() */


void CheckRealError(int Err, char *func_name) {

	if (Err) {
		printf("DOS386 real mode call error: %s\n",func_name);
		exit(1);
	} /* if */

} /* CheckRealError() */




UDIError CAT(UDIPConnect,UDI12FUNCVER)(
			REALPTR UDIConnectAddr, 
			char *Configuration,		/* In  */
			UDISessionId *Session,		/* Out */
			DOSTerm _far *TermStruct)	/* In  */
{
	int			err;
	UDISessionId _far	*SessionPtr;
	UDIError 		ConnectErr;
	USHORT			rm_seg,rm_size;

	_Packed struct {
		REALPTR		Configuration;
		REALPTR  	Session;
		REALPTR		TermStruct;
	} params;


	if (!buffer_allocated) {
		/* Need to get address of conventional memory area for passing parameters.
		 * This will set it for future use everywhere, not just in this function. 
		 * rm_address is the protected (32 bit) pointer to the real mode parameter.
		 * passing area.  rm_buffer_addr is the real mode pointer to the same buffer.
		 */
		err = _dx_real_alloc(REAL_BUFFER_SIZE,&rm_seg,&rm_size);

		if (err) {
			printf("Unable to allocate real-mode parameter transfer area (_dx_real_alloc)\n");
			exit(0);
		}
		/* rm_seg is the real mode paragraph (segment).
		 * Build rm_buffer_addr to be the full real mode pointer (seg:ofst)
		 */
		RP_SET(rm_buffer_addr, 0, rm_seg);
		/*
		 * rm_address will be the protected pointer to that same buffer
		 */
		rm_address       = (USHORT _far *)REAL_TO_PROT(rm_buffer_addr);
		rm_end_address   = (USHORT _far *) (((char _far *)rm_address) + REAL_BUFFER_SIZE*16);
		buffer_allocated = 1;
	}
	
	
	/* Get pointers to locations where passed by reference parameters 
	 * will be stored in the parameter passing buffer area.  The first
	 * parameter is always at rm_buffer (= rm_buffer_addr in real mode).
	 */

	/* NOTE: see comments under UDIPDisconnect for explanation of why
	 * we don't copy TermStruct even though it's an in parameter.
	 */
	SessionPtr = (UDISessionId _far *)((char _far *)rm_address + strlen(Configuration)+1);
				       
	if (BufferSizeCheck((FARPTR)(SessionPtr + sizeof(UDISessionId)),"UDIPConnect" UDI12FUNCVERSTR,PRINT_ON)) {
		return UDIErrorIPCInternal;
	} /* if */

	/* Move input parameters which are passed by reference into paramter buffer area. */
	_fmemmove(rm_address,NEARPTR_TO_FARPTR(Configuration),strlen(Configuration)+1);
	_fmemmove(SessionPtr,NEARPTR_TO_FARPTR(Session),sizeof(UDISessionId));


	/* Put actual parameters into packed structure for passing to real
	 * mode function.
	 */
	params.Configuration = rm_buffer_addr;
	params.Session       = PROT_TO_REAL((FARPTR) SessionPtr);
	params.TermStruct    = PROT_TO_REAL((FARPTR)TermStruct);


	/* Call the real-mode function with the address of the function,
	 * the number of bytes in the packed structure and the address of
	 * the structure.
	 */

	ConnectErr = REALCALL(UDIConnectAddr,params);

	CheckRealError(ConnectErr,"UDIConnect" UDI12FUNCVERSTR);

	/* Copy output parameters from parameter passing area back to protected space
	 */
	_fmemmove(NEARPTR_TO_FARPTR(Session),SessionPtr,sizeof(UDISessionId));


	return FUNC_VAL;


} /* UDIPConnect (UDI 1.2) */


#ifdef UDI13

/* 4/93 MDT - This function is needed only for UDI 1.3 and greater 
 *            implementations.  This code should be checked when the
 *            final specification for UDI 1.3 becomes available.
 */

UDIError UDIPConnect_13(
	REALPTR 	UDIConnectAddr, 
	char		*Configuration, 	/* In  */
	UDISessionId	*Session, 		/* Out */
	DOSTerm 	*TermStruct,		/* In  */
	UDIUInt32	DFEIPCId,		/* In  1.3 */
	UDIUInt32	*TIPIPCId,		/* Out 1.3 */
	struct UDIDFEVecRec *DFEVecRec		/* In  1.3 */
	)
{
	int			err;
	UDISessionId _far	*SessionPtr;
	UDIUInt32 _far		*TIPIPCIdPtr;
	UDIError 		ConnectErr;
	USHORT			rm_seg,rm_size;

	_Packed struct {
		REALPTR		Configuration;
		REALPTR  	Session;
		REALPTR		TermStruct;
		UDIUInt32	DFEIPCId;
		REALPTR		TIPIPCId;
		REALPTR		DFEVecRec;
	} params;

	
	if (!buffer_allocated) {
		/* Need to get address of conventional memory area for passing parameters.
		 * This will set it for future use everywhere, not just in this function. 
		 * rm_address is the protected (32 bit) pointer to the real mode parameter.
		 * passing area.  rm_buffer_addr is the real mode pointer to the same buffer.
		 */
		err = _dx_real_alloc(REAL_BUFFER_SIZE,&rm_seg,&rm_size);
		if (err) {
			printf("Unable to allocate real-mode parameter transfer area (_dx_real_alloc)\n");
			exit(0);
		}
		/* rm_seg is the real mode paragraph (segment).
		 * Build rm_buffer_addr to be the full real mode pointer (seg:ofst)
		 */
		RP_SET(rm_buffer_addr, 0, rm_seg);
		/*
		 * rm_address will be the protected pointer to that same buffer
		 */
		rm_address       = REAL_TO_PROT(rm_buffer_addr);
		rm_end_address   = (USHORT *) (((char *)rm_address) + REAL_BUFFER_SIZE*16);
		buffer_allocated = 1;
	}
	
	
	/* Get pointers to locations where passed by reference parameters 
	 * will be stored in the parameter passing buffer area.  The first
	 * parameter is always at rm_buffer (= rm_buffer_addr in real mode).
	 */

	/* NOTE: see comments under UDIPDisconnect for explanation of why
	 * we don't copy TermStruct even though it's an in parameter.
	 */
	SessionPtr = (UDISessionId _far *)((char _far *)rm_address + strlen(Configuration)+1);
	TIPIPCIdPtr = (UDIUInt32 _far *) (SessionPtr + sizeof(UDISessionId));
	
	if (BufferSizeCheck((FARPTR)(TIPIPCIdPtr + sizeof(UDIUInt32)),"UDIPConnect (1.3)")) {
		return UDIErrorIPCInternal;
	}

	/* Move input parameters which are passed by reference into paramter buffer area. */
	_fmemmove(rm_address,NEARPTR_TO_FARPTR(Configuration),strlen(Configuration)+1);

	/* Put actual parameters into packed structure for passing to real
	 * mode function.
	 */
	params.Configuration = rm_buffer_addr;
	params.Session       = PROT_TO_REAL((FARPTR)SessionPtr);
	params.TermStruct    = PROT_TO_REAL((FARPTR)TermStruct);
	params.DFEIPCId	     = DFEIPCId;
	params.TIPIPCId	     = PROT_TO_REAL(TIPIPCIdPtr);
	params.DFEVecRec     = PROT_TO_REAL((FARPTR)DFEVecRec);


	/* Call the real-mode function with the address of the function,
	 * the number of bytes in the packed structure and the address of
	 * the structure.
	 */

	ConnectErr = REALCALL(UDIConnectAddr,params);

	CheckRealError(ConnectErr,"UDIConnect (1.3)");

	/* Copy output parameters from parameter passing area back to protected space
	 */
	_fmemmove(NEARPTR_TO_FARPTR(Session),SessionPtr,sizeof(UDISessionId));
	_fmemmove(NEARPTR_TO_FARPTR(TIPIPCId),TIPIPCIdPtr,sizeof(UDIUInt32));

	return FUNC_VAL;


} /* UDIPConnect_13 */

#endif /* UDI13 */


#define TIPSTRLEN 80


UDIError CAT(UDIPCapabilities,UDI12FUNCVER) (
	REALPTR		UDICapabilitiesAddr,
	UDIUInt32	*TIPId,			/* Out */
	UDIUInt32	*TargetId,		/* Out */
	UDIUInt32	DFEId,			/* In */
	UDIUInt32	DFE,			/* In */
	UDIUInt32	*TIP,			/* Out */
	UDIUInt32	*DFEIPCId,		/* Out */
	UDIUInt32	*TIPIPCId,		/* Out */
	char		*TIPString		/* Out */
  	)
{
	UDIUInt32 _far	*TargetIdPtr;
	UDIUInt32 _far	*TIPPtr;
	UDIUInt32 _far	*DFEIPCIdPtr;
	UDIUInt32 _far	*TIPIPCIdPtr;
	UDIUInt32 _far	*TIPStringPtr;
	UDIUInt32 _far	*endPtr;
	UDIError 	Err;

	/* Structure for passing parameters to real mode function in TIP */
	_Packed struct {
		REALPTR		TIPId;
		REALPTR		TargetId;
		UDIUInt32	DFEId;
		UDIUInt32	DFE;
		REALPTR		TIP;
		REALPTR		DFEIPCId;
		REALPTR		TIPIPCId;
		REALPTR		TIPString;
	} params;


	/* Get pointers to locations where passed by reference parameters 
	 * will be stored in the parameter passing buffer area.  The first
	 * parameter is always at rm_address.
	 */

	TargetIdPtr = (UDIUInt32 _far *)((char _far *)rm_address + sizeof(UDIUInt32));
	TIPPtr = TargetIdPtr + sizeof(UDIUInt32);
	DFEIPCIdPtr = TIPPtr + sizeof(UDIUInt32);
	TIPIPCIdPtr = DFEIPCIdPtr + sizeof(UDIUInt32);
	TIPStringPtr = TIPIPCIdPtr + sizeof(UDIInt32);
	endPtr = TIPStringPtr + TIPSTRLEN;

	if (BufferSizeCheck((FARPTR)endPtr,"UDICapabilities" UDI12FUNCVERSTR,PRINT_ON)) {
		return UDIErrorIPCLimitation;		
	} /* if */
	
	/* Move parameters into paramter buffer area. */
	_fmemmove(rm_address,NEARPTR_TO_FARPTR(TIPId),sizeof(UDIUInt32));     
	_fmemmove(TargetIdPtr,NEARPTR_TO_FARPTR(TargetId),sizeof(UDIUInt32));
	_fmemmove(TIPPtr,NEARPTR_TO_FARPTR(TIP),sizeof(UDIUInt32));
	_fmemmove(DFEIPCIdPtr,NEARPTR_TO_FARPTR(DFEIPCId),sizeof(UDIUInt32));
	_fmemmove(TIPIPCIdPtr,NEARPTR_TO_FARPTR(TIPIPCId),sizeof(UDIInt32));
	_fmemmove(TIPStringPtr,NEARPTR_TO_FARPTR(TIPString),TIPSTRLEN);


	params.TIPId = rm_buffer_addr;
	params.TargetId = PROT_TO_REAL((FARPTR)TargetIdPtr);
	params.DFEId = DFEId;
	params.DFE = DFE;
	params.TIP = PROT_TO_REAL((FARPTR)TIPPtr);
	params.DFEIPCId = PROT_TO_REAL((FARPTR)DFEIPCIdPtr);
	params.TIPIPCId = PROT_TO_REAL((FARPTR)TIPIPCIdPtr);
	params.TIPString = PROT_TO_REAL((FARPTR)TIPStringPtr);


	Err = REALCALL(UDICapabilitiesAddr,params);

	CheckRealError(Err,"UDICapabilities" UDI12FUNCVERSTR);

	_fmemmove(NEARPTR_TO_FARPTR(TargetId),TargetIdPtr,sizeof(UDIUInt32));
	_fmemmove(NEARPTR_TO_FARPTR(TIP),TIPPtr,sizeof(UDIUInt32));
	_fmemmove(NEARPTR_TO_FARPTR(DFEIPCId),DFEIPCIdPtr,sizeof(UDIUInt32));
	_fmemmove(NEARPTR_TO_FARPTR(TIPIPCId),TIPIPCIdPtr,sizeof(UDIInt32));
	_fmemmove(NEARPTR_TO_FARPTR(TIPString),TIPStringPtr,TIPSTRLEN);
	_fmemmove(NEARPTR_TO_FARPTR(TIPId),(UDIUInt32 _far *)rm_address,sizeof(UDIUInt32));

	return FUNC_VAL;

}  /* UDIPCapabilities() */



#ifdef UDI13

/* UDI 1.3 version of UDIPCapabilities */
UDIError UDIPCapabilities_13 (
	REALPTR		UDICapabilitiesAddr,
	UDIUInt32	*TIPId,			/* Out */
	UDIUInt32	*TargetId,		/* Out */
	UDIUInt32	DFEId,			/* In */
	UDIUInt32	DFE,			/* In */
	UDIUInt32	*TIP,			/* Out */
	UDIUInt32	*DFEIPCId,		/* Out */
	UDIUInt32	*TIPIPCId,		/* Out */
	char		*TIPString,		/* Out */
	UDISizeT	BufSize,		/* In  1.3 */
	UDISizeT	*CountDone,		/* Out 1.3 */
	UDISessionId	connection_id
  	)
{
	UDIUInt32 _far	*TargetIdPtr;
	UDIUInt32 _far	*TIPPtr;
	UDIUInt32 _far	*DFEIPCIdPtr;
	UDIUInt32 _far	*TIPIPCIdPtr;
	UDIUInt32 _far	*TIPStringPtr;
	UDIUInt32 _far	*endPtr;
	UDISizeT	*CountDonePtr;
	UDIError 	ConnectErr;

	/* Structure for passing parameters to real mode function in TIP */
	_Packed struct {
		REALPTR		TIPId;
		REALPTR		TargetId;
		UDIUInt32	DFEId;
		UDIUInt32	DFE;
		REALPTR		TIP;
		REALPTR		DFEIPCId;
		REALPTR		TIPIPCId;
		REALPTR		TIPString;
		UDISizeT	BufSize;
		REALPTR		CountDone;
		UDISessionId	connection_id;
	} params;


	/* Get pointers to locations where passed by reference parameters 
	 * will be stored in the parameter passing buffer area.  The first
	 * parameter is always at rm_address.
	 */

	TargetIdPtr  = (UDIUInt32 _far *)((char _far *)rm_address + sizeof(UDIUInt32));
	TIPPtr       = TargetIdPtr + sizeof(UDIUInt32);
	DFEIPCIdPtr  = TIPPtr + sizeof(UDIUInt32);
	TIPIPCIdPtr  = DFEIPCIdPtr + sizeof(UDIUInt32);
	TIPStringPtr = TIPIPCIdPtr + sizeof(UDIInt32);
	CountDonePtr = (UDISizeT _far *) (TIPStringPtr + strlen(TIPString) + 1);
	endPtr       = (UDIUInt32 _far *) (CountDonePtr + sizeof(UDISizeT));

	if (BufferSizeCheck((FARPTR)endPtr,"UDICapabilities (1.3)",PRINT_ON) {
		return UDIErrorIPCLimitation;
	} /* if */

	
	/* Move parameters into paramter buffer area. */
	_fmemmove(rm_address,NEARPTR_TO_FARPTR(TIPId),sizeof(UDIUInt32));			/* TIPId */
	_fmemmove(TargetIdPtr, NEARPTR_TO_FARPTR(TargetId),sizeof(UDIUInt32));
	_fmemmove(TIPPtr, NEARPTR_TO_FARPTR(TIP),sizeof(UDIUInt32));
	_fmemmove(DFEIPCIdPtr, NEARPTR_TO_FARPTR(DFEIPCId),sizeof(UDIUInt32));
	_fmemmove(TIPIPCIdPtr, NEARPTR_TO_FARPTR(TIPIPCId),sizeof(UDIInt32));
	_fmemmove(TIPStringPtr, NEARPTR_TO_FARPTR(TIPString),strlen(TIPString)+1);
	_fmemmove(CountDonePtr, NEARPTR_TO_FARPTR(CountDone),sizeof(UDISizeT));


	params.TIPId         = rm_buffer_addr;
	params.TargetId      = PROT_TO_REAL((FARPTR)TargetIdPtr);
	params.DFEId         = DFEId;
	params.DFE           = DFE;
	params.TIP           = PROT_TO_REAL((FARPTR)TIPPtr);
	params.DFEIPCId      = PROT_TO_REAL((FARPTR)DFEIPCIdPtr);
	params.TIPIPCId      = PROT_TO_REAL((FARPTR)TIPIPCIdPtr);
	params.TIPString     = PROT_TO_REAL((FARPTR)TIPStringPtr);
	params.BufSize	     = BufSize;
	params.CountDone     = PROT_TO_REAL((FARPTR)CountDonePtr);
	params.connection_id = connection_id;


	ConnectErr = REALCALL(UDICapabilitiesAddr,params);

	CheckRealError(ConnectErr,"UDICapabilities (1.3)");


	_fmemmove(NEARPTR_TO_FARPTR(TIPId),     rm_address,   sizeof(UDIUInt32));
	_fmemmove(NEARPTR_TO_FARPTR(TargetId),  TargetIdPtr,  sizeof(UDIUInt32));
	_fmemmove(NEARPTR_TO_FARPTR(TIP),       TIPPtr,       sizeof(UDIUInt32));
	_fmemmove(NEARPTR_TO_FARPTR(DFEIPCId),  DFEIPCIdPtr,  sizeof(UDIUInt32));
	_fmemmove(NEARPTR_TO_FARPTR(TIPIPCId),  TIPIPCIdPtr,  sizeof(UDIInt32));
	_fmemmove(NEARPTR_TO_FARPTR(TIPString), TIPStringPtr, strlen(TIPString)+1);
	_fmemmove(NEARPTR_TO_FARPTR(CountDone), CountDonePtr, sizeof(CountDone));

	if (*CountDone <= BufSize)
		_fmemmove(NEARPTR_TO_FARPTR(TIPString),TIPStringPtr,*CountDone);
	else {
		_fmemmove(NEARPTR_TO_FARPTR(TIPString),TIPStringPtr,BufSize);
		SIZE_ERROR(*CountDone, BufSize, "UDIPCapabilities (1.3)");
	}



	return FUNC_VAL;

}  /* UDIPCapabilities_13() */

#endif /* UDI13 */


UDIError UDIPGetErrorMsg(
	REALPTR		UDIGetErrorMessageAddr,
	UDIError	ErrorCode,		/* In  */
	UDISizeT	MsgSize,		/* In  */
	char		*Msg,			/* Out */
	UDISizeT	*CountDone		/* Out */
	)
{
	UDIError	Err;
	UDISizeT _far	*CountDonePtr;
	long		Buffer_Adjustment;

	_Packed struct {
		UDIError	ErrorCode;
		UDISizeT	MsgSize;
		REALPTR		Msg;
		REALPTR		CountDone;
	} params;


	CountDonePtr = (UDISizeT _far *)(rm_address + MsgSize);

	if ((Buffer_Adjustment = BufferSizeCheck((FARPTR)(CountDonePtr + sizeof(UDISizeT)),"UDIPGetErrorMsg",PRINT_ON))) {
		if (MsgSize <= Buffer_Adjustment)
			return UDIErrorIPCLimitation;
		MsgSize -= Buffer_Adjustment;
	} /* if */

	/* Don't need to copy anything into the real mode parameter 
	 * buffer area for this call since there are no pointer "in"
	 * parameters.				     
	 */

	params.ErrorCode = ErrorCode;
	params.MsgSize   = MsgSize;
	params.Msg       = rm_buffer_addr;
	params.CountDone = PROT_TO_REAL((FARPTR)CountDonePtr);

	Err = REALCALL(UDIGetErrorMessageAddr,params);

	CheckRealError(Err,"UDIGetErrorMessage");

	_fmemmove(NEARPTR_TO_FARPTR(CountDone),CountDonePtr,sizeof(UDISizeT));
	if (*CountDone <= MsgSize)
		_fmemmove(NEARPTR_TO_FARPTR(Msg),rm_address,*CountDone);
	else {
		_fmemmove(NEARPTR_TO_FARPTR(Msg),rm_address,MsgSize);
		SIZE_ERROR(*CountDone, MsgSize, "UDIPGetErrorMessage");
	}
	
	return FUNC_VAL;

} /* UDIPGetErrorMessage */



UDIError UDIPSetCurrentConnection(
	REALPTR		UDISetCurrentConnectionAddr,
	UDISessionId	Session			/* In  */
	)
{
	UDIError	Err;

	Err = REALCALL(UDISetCurrentConnectionAddr,Session);

	CheckRealError(Err,"UDISetCurrentConnection");

	return FUNC_VAL;

} /* UDIPSetCurrentConnection() */



UDIError UDIPDisconnect(
	REALPTR		UDIDisconnectAddr,
	UDISessionId	Session,		/* In  */
	UDIBool		Terminate,		/* In  */
	DOSTerm _far	*TermStruct	/* In  */
	)
{
	UDIError	Err;
	DOSTerm         t;

	_Packed struct {
		UDISessionId	Session;
		UDIBool		Terminate;
		REALPTR		Term;
	} params;

	_fmemmove(NEARPTR_TO_FARPTR(&t),TermStruct,sizeof(TermStruct));

	/* The only pointer to be passed is a pointer to TermStruct
	 * but since TermStruct is already in real memory, we do
	 * not want to copy it (The TIP must store its registers into
	 * the original TermStruct, not the copy)
	 */

	/* For this call just need to pack everything into a structure,
	 * then do the call since there are no "out" parameters and no
	 * pointers to "in" parameters.
	 */
	params.Session   = Session;
	params.Terminate = Terminate;
	params.Term      = PROT_TO_REAL((FARPTR)TermStruct);	/* just convert to real */

	Err = REALCALL(UDIDisconnectAddr,params);

	CheckRealError(Err,"UDIDisconnect");

	return FUNC_VAL;

} /* UDIPDisconnect() */



UDIError UDIPGetTargetConfig(
	REALPTR		UDIGetTargetConfigAddr,
	UDIMemoryRange	KnownMemory[],		/* Out */
	UDIInt		*NumberOfRanges,	/* In/Out */
	UDIUInt32	ChipVersions[],		/* Out */
	UDIInt		*NumberOfChips		/* In/Out */
	)
{
	_Packed struct {
		REALPTR		KnownMemory;
		REALPTR		NumberOfRanges;
		REALPTR		ChipVersions;
		REALPTR		NumberOfChips;
	} params;

	UDIError		Err;
	UDIInt _far		*NumberOfRangesPtr;
	UDIUInt32 _far		*ChipVersionsPtr;
	UDIInt _far		*NumberOfChipsPtr;

	/* Get addresses in parameter passing buffer to store parameters
	 * which are passed-by-reference.
	 */
	 NumberOfRangesPtr = (UDIInt _far *) (rm_address + *NumberOfRanges * sizeof(UDIMemoryRange));
	 ChipVersionsPtr   = (UDIUInt32 _far *) (NumberOfRangesPtr + sizeof(UDIInt));
	 NumberOfChipsPtr  = (UDIInt _far *) (ChipVersionsPtr + *NumberOfChips * sizeof(UDIUInt32));



	if (BufferSizeCheck((FARPTR)(NumberOfChipsPtr + sizeof(UDIInt)),"UDIPGetTargetConfig",PRINT_ON)) {
		return UDIErrorIPCLimitation;
	} /* if */

	/* Copy parameters which are passed-by-reference to parameter
	 * passing buffer.  Only "In" data needs to be copied. 
	 */
	 _fmemmove(NumberOfRangesPtr,NEARPTR_TO_FARPTR(NumberOfRanges),sizeof(UDIInt));
	 _fmemmove(NumberOfChipsPtr,NEARPTR_TO_FARPTR(NumberOfChips),sizeof(UDIInt));

	/* Put data into packed structure. */
	params.KnownMemory    = rm_buffer_addr;
	params.NumberOfRanges = PROT_TO_REAL((FARPTR)NumberOfRangesPtr);
	params.ChipVersions   = PROT_TO_REAL((FARPTR)ChipVersionsPtr);
	params.NumberOfChips  = PROT_TO_REAL((FARPTR)NumberOfChipsPtr);

	Err = REALCALL(UDIGetTargetConfigAddr,params);

	CheckRealError(Err,"UDIGetTargetConfig");

	/* Put data back into protected mode program address. */
	_fmemmove(NEARPTR_TO_FARPTR(KnownMemory),rm_address,*NumberOfRanges * sizeof(UDIMemoryRange));
	_fmemmove(NEARPTR_TO_FARPTR(NumberOfRanges),NumberOfRangesPtr, sizeof(UDIInt));
	_fmemmove(NEARPTR_TO_FARPTR(ChipVersions), ChipVersionsPtr, *NumberOfChips * sizeof(UDIUInt32));
	_fmemmove(NEARPTR_TO_FARPTR(NumberOfChips), NumberOfChipsPtr, sizeof(UDIInt));

	return FUNC_VAL;

} /* UDIPGetTargetConfig() */



UDIError UDIPCreateProcess(
	REALPTR		UDICreateProcessAddr,
	UDIPId		*PId			/* Out */
	)
{
	UDIError	Err;


	if (BufferSizeCheck((FARPTR)(rm_address + sizeof(UDIPId)),"UDIPCreateProcess",PRINT_ON)) {
		return UDIErrorIPCLimitation;
	} /* if */

	/* Copy passed-by-reference information to parameter passing buffer. */
	_fmemmove(rm_address,NEARPTR_TO_FARPTR(PId),sizeof(UDIPId));

	/* Don't need to create structure since there is only one parameter. */
	Err = REALCALL(UDICreateProcessAddr,rm_buffer_addr);

	CheckRealError(Err,"UDICreateProcess");

	/* Copy "out" data back to protected mode program address. */
	_fmemmove(NEARPTR_TO_FARPTR(PId),rm_address,sizeof(UDIPId));

	return FUNC_VAL;

} /* UDIPCreateProcess() */



UDIError UDIPSetCurrentProcess(
	REALPTR		UDISetCurrentProcessAddr,
	UDIPId		PId			/* In  */
	)
{
	UDIError	Err;

	
	Err = REALCALL(UDISetCurrentProcessAddr,PId);

	CheckRealError(Err,"UDISetCurrentProcess");

	return FUNC_VAL;

} /* UDIPSetCurrentProcess() */



UDIError UDIPInitializeProcess(
	REALPTR		UDIInitializeProcessAddr,
	UDIMemoryRange	ProcessMemory[],	/* In  */
	UDIInt		NumberOfRanges,		/* In  */
	UDIResource	EntryPoint,		/* In  */
	CPUSizeT	*StackSizes,		/* In  */
	UDIInt		NumberOfStacks,		/* In  */
	char		*ArgString		/* In  */
	)
{
	_Packed struct {
		REALPTR		ProcessMemory;
		UDIInt		NumberOfRanges;
		UDIResource	EntryPoint;
		REALPTR		StackSizes;
		UDIInt		NumberOfStacks;
		REALPTR		ArgString;
	} params;

	/* Pointers to variables stored in the parameter passing buffer. */
	CPUSizeT _far	*StackSizesPtr;
	char _far	*ArgStringPtr;
	UDIError	Err;


	StackSizesPtr = (CPUSizeT _far *) (rm_address + NumberOfRanges*sizeof(UDIMemoryRange));
	ArgStringPtr  = (char _far *) (StackSizesPtr + NumberOfStacks * sizeof(CPUSizeT));

	if (BufferSizeCheck((FARPTR)(ArgStringPtr + strlen(ArgString) + 1),"UDIPInitializeProcess",PRINT_ON)) {
		return UDIErrorIPCLimitation;
	} /* if */

	/* Move things passed by reference into the parameter passing buffer. */
	_fmemmove(rm_address,NEARPTR_TO_FARPTR(ProcessMemory),NumberOfRanges*sizeof(UDIMemoryRange));
	_fmemmove(StackSizesPtr,NEARPTR_TO_FARPTR(StackSizes),NumberOfStacks * sizeof(CPUSizeT));
	_fmemmove(ArgStringPtr, NEARPTR_TO_FARPTR(ArgString), strlen(ArgString)+1);

	/* Fill the packed array for passing to the real mode function. */
	params.ProcessMemory  = rm_buffer_addr;
	params.NumberOfRanges = NumberOfRanges;
	params.EntryPoint     = EntryPoint;
	params.StackSizes     = PROT_TO_REAL((FARPTR)StackSizesPtr);
	params.NumberOfStacks = NumberOfStacks;
	params.ArgString      = PROT_TO_REAL((FARPTR)ArgStringPtr);
	
	/* Call the real mode function. */
	Err = REALCALL(UDIInitializeProcessAddr,params);

	CheckRealError(Err,"UDIInitializeProcess");

	/* Don't need to copy anything back since all of the parameters are
	 * "in" only.
	 */

	return FUNC_VAL;

} /* UDIPInitializeProcess() */



UDIError UDIPDestroyProcess(
	REALPTR		UDIDestroyProcessAddr,
	UDIPId		PId			/* In  */
	)
{

	UDIError	Err;

	Err = REALCALL(UDIDestroyProcessAddr,PId);

	CheckRealError(Err,"UDIDestroyProcess");

	return FUNC_VAL;

} /* UDIPDestroyProcess() */



UDIError UDIPRead(
	REALPTR		UDIReadAddr,
	UDIResource	From,			/* In  */
	UDIHostMemPtr	To,			/* Out */
	UDICount	Count,			/* In  */
	UDISizeT	Size,			/* In  */
	UDICount	*CountDone,		/* Out */
	UDIBool		HostEndian		/* In  */
	)
{
	_Packed struct {
		UDIResource	From;
		REALPTR		To;
		UDICount	Count;
		UDISizeT	Size;
		REALPTR		CountDone;
		UDIBool		HostEndian;
	} params;

	UDIError	Err;
	UDICount _far	*CountDonePtr;

	/* Looping control variables */
	UDICount	TotalDone=0;	/* Total number of items xfered so far */
	UDICount	CurrentCount;	/* Number of items to be xfered this pass */
	UDIResource	CurrentFrom;	/* Current pointer into From area */
	char *		CurrentTo;	/* Current pointer into To area */
	UDICount	BufAdjust;	/* size of buffer overflow in bytes */
	UDICount	CurrentDone;	/* The actual number of items xfered this pass */


	CurrentTo    = (char *) To;
	CurrentFrom  = From;
	CurrentCount = Count;
	do {
		CountDonePtr = (UDICount _far *) (rm_address + CurrentCount * Size);

		/* Check to see if transfer needs to be broken into smaller pieces */
		BufAdjust = BufferSizeCheck((FARPTR)(CountDonePtr + sizeof(UDICount)),"UDIPRead",PRINT_OFF); 
		if (BufAdjust)  {
			CurrentCount = (rm_end_address - rm_address - sizeof(UDICount)) / Size ;
			CountDonePtr = (UDICount _far *) (rm_end_address - sizeof(UDICount));
		}
		
		/* Copy parameters into packed structure. */
		params.From       = CurrentFrom;
		params.To         = rm_buffer_addr;
		params.Count      = CurrentCount;
		params.Size       = Size;
		params.CountDone  = PROT_TO_REAL((FARPTR)CountDonePtr);
		params.HostEndian = HostEndian;

		Err = REALCALL(UDIReadAddr,params);

		CheckRealError(Err,"UDIRead");

		_fmemmove(NEARPTR_TO_FARPTR(&CurrentDone),CountDonePtr,sizeof(UDICount));

		/* Increment the TotalDone by the actual number of items xfered as
		 * returned from the function.
		 */
		TotalDone += CurrentDone;

		if ((CurrentDone <= CurrentCount) && (CurrentDone >= 0))
			_fmemmove(NEARPTR_TO_FARPTR(CurrentTo),rm_address,CurrentDone * Size);
		else {
			_fmemmove(NEARPTR_TO_FARPTR(CurrentTo),rm_address, CurrentCount * Size);
			SIZE_ERROR(CurrentDone, CurrentCount, "UDIPRead");
		}

		/* Update looping variables for possible next pass */
		CurrentFrom.Offset += CurrentCount * Size;
		CurrentTo += CurrentCount * Size;
		CurrentCount = Count - TotalDone; 

	} while ((TotalDone < Count) & (FUNC_VAL == UDINoError));

	*CountDone = TotalDone;

	return FUNC_VAL;

} /* UDIPRead() */


UDIError UDIPWrite(
	REALPTR		UDIWriteAddr,
	UDIHostMemPtr	From,			/* In  */
	UDIResource	To,			/* In  */
	UDICount	Count,			/* In  */
	UDISizeT	Size,			/* In  */
	UDICount	*CountDone,		/* Out */
	UDIBool		HostEndian		/* In  */
	)
{
	_Packed struct {
		REALPTR		From;
		UDIResource	To;
		UDICount	Count;
		UDISizeT	Size;
		REALPTR		CountDone;
		UDIBool		HostEndian;
	} params;

	UDIError	Err;
	UDICount _far	*CountDonePtr;

	/* Looping control variables */
	UDICount	TotalDone=0;	/* Total number of items xfered so far */
	UDICount	CurrentCount;	/* Number of items to be xfered this pass */
	char *		CurrentFrom;	/* Current pointer into From area */
	UDIResource	CurrentTo;	/* Current pointer into To area */
	UDICount	BufAdjust;	/* size of buffer overflow in bytes */
	UDICount	CurrentDone;	/* The actual number of items xfered this pass */


	CurrentTo    = To;
	CurrentFrom  = (char *) From;
	CurrentCount = Count;

	do {
		CountDonePtr = (UDICount _far *) (rm_address + Size * Count);

		/* Check to see if transfer needs to be broken into smaller pieces. */
		BufAdjust = BufferSizeCheck((FARPTR)(CountDonePtr + sizeof(UDICount)),"UDIPWrite",PRINT_ON);
		if (BufAdjust) {
			CurrentCount = (rm_end_address - rm_address - sizeof(UDICount)) / Size;
			CountDonePtr = (UDICount _far *) (rm_end_address - sizeof(UDICount));
		} /* if */

		/* Move data passed by reference into the parameter passing buffer
		 * area in conventional memory. 
		 */
		_fmemmove(rm_address, NEARPTR_TO_FARPTR(CurrentFrom), Size * CurrentCount);

		/* Move data to packed structure for passing to real mode function. */
		params.From	  = rm_buffer_addr;
		params.To	  = CurrentTo;
		params.Count	  = CurrentCount;
		params.Size	  = Size;
		params.CountDone  = PROT_TO_REAL((FARPTR)CountDonePtr);
		params.HostEndian = HostEndian;

		Err = REALCALL(UDIWriteAddr,params);
	
		CheckRealError(Err,"UDIWrite");
		
		/* Move "out" data back into protected mode memory area. */
		_fmemmove(NEARPTR_TO_FARPTR(&CurrentDone),CountDonePtr,sizeof(UDICount));

		/* Increment the ToralDone by the actual number of items xfered as
		 * returned from the function.
		 */
		TotalDone += CurrentDone;

		/* Update looping variables for possible next pass */
		CurrentFrom += CurrentCount * Size;
		CurrentTo.Offset += CurrentCount * Size;
		CurrentCount = Count - TotalDone;

	} while ((TotalDone < Count) & (FUNC_VAL == UDINoError));

	/* Return the total number of items xfered */
	*CountDone = TotalDone;

	return FUNC_VAL;

} /* UDIPWrite() */


UDIError UDIPCopy(
	REALPTR		UDICopyAddr,
	UDIResource	From,			/* In  */
	UDIResource	To,			/* In  */
	UDICount	Count,			/* In  */
	UDISizeT	Size,			/* In  */
	UDICount	*CountDone,		/* Out */
	UDIBool		Direction		/* In  */
	)
{
	_Packed struct {
		UDIResource	From;
		UDIResource	To;
		UDICount	Count;
		UDISizeT	Size;
		REALPTR		CountDone;
		UDIBool		Direction;
	} params;

	UDIError	Err;

	/* Copy data into packed structure for passing to real mode funciton. */
	params.From	= From;
	params.To	= To;
	params.Count	= Count;
	params.Size	= Size;
	params.CountDone= rm_buffer_addr;
	params.Direction= Direction;

	Err = REALCALL(UDICopyAddr,params);

	CheckRealError(Err,"UDICopy");

	_fmemmove(NEARPTR_TO_FARPTR(CountDone), rm_address, sizeof(UDICount));

	return FUNC_VAL;

} /* UDIPCopy() */



UDIError UDIPExecute(
	REALPTR		UDIExecuteAddr
	)
{
	UDIError	Err;

	Err = _dx_call_real(UDIExecuteAddr, &real_regs, 0);

	CheckRealError(Err,"UDIExecute");

	return FUNC_VAL;

} /* UDIPExecute() */



UDIError UDIPStep(
	REALPTR		UDIStepAddr,
	UDIUInt32	Steps,			/* In  */
	UDIStepType	StepType,		/* In  */
	UDIRange	Range			/* In  */
	)
{
	UDIError	Err;

	_Packed struct {
		UDIUInt32	Steps;
		UDIStepType	StepType;
		UDIRange	Range;
	} params;

	/* Since nothing is passed by reference, don't need to use
	 * buffer transfer area.
	 */

	/* Copy passed parameters into packed structure */
	params.Steps    = Steps;
	params.StepType = StepType;
	params.Range    = Range;

	Err = REALCALL(UDIStepAddr,params);

	CheckRealError(Err,"UDIStep");

	return FUNC_VAL;

} /* UDIPStep() */



UDIError UDIPStop(
	REALPTR		UDIStopAddr
	)
{
	UDIError	Err;

	Err = _dx_call_real(UDIStopAddr, &real_regs, 0);

	CheckRealError(Err,"UDIStop");

	return FUNC_VAL;

} /* UDIPStop() */



UDIError UDIPWait(
	REALPTR		UDIWaitAddr,
	UDIInt32	MaxTime,		/* In  */
	UDIPId		*PId,			/* Out */
	UDIUInt32	*StopReason		/* Out */
	)
{
	UDIError	Err;
	UDIUInt32 _far	*StopReasonPtr;

	_Packed struct {
		UDIInt32	MaxTime;
		REALPTR		PId;
		REALPTR		StopReason;
	} params;

	/* Since only "out" parameters are passed by reference, don't
	 * need to copy anything into the parameter passing buffer before
	 * the call.  Do need to set up pointer for StopReason though.
	 */
	StopReasonPtr = (UDIUInt32 _far *) (rm_address + sizeof(UDIPId));

	if (BufferSizeCheck((FARPTR)(StopReasonPtr + sizeof(UDIUInt32)),"UDIPWait",PRINT_ON)) {
		return UDIErrorIPCLimitation;
	} /* if */

	params.MaxTime    = MaxTime;
	params.PId        = rm_buffer_addr;
	params.StopReason = PROT_TO_REAL((FARPTR)StopReasonPtr);

	Err = REALCALL(UDIWaitAddr,params);

	CheckRealError(Err,"UDIWait");

	/* Need to copy "out" parameter data back into protected mode
	 * address space. 
	 */
	_fmemmove(NEARPTR_TO_FARPTR(PId),rm_address,sizeof(UDIPId));
	_fmemmove(NEARPTR_TO_FARPTR(StopReason),StopReasonPtr,sizeof(UDIUInt32));

	return FUNC_VAL;

} /* UDIPWait() */



UDIError UDIPSetBreakpoint(
	REALPTR		UDISetBreakpointAddr,
	UDIResource	Addr,	  		/* In  */
	UDIInt32	PassCount,		/* In  */
	UDIBreakType	Type,			/* In  */
	UDIBreakId	*BreakId		/* Out */
	)
{
	UDIError	Err;

	_Packed struct {
		UDIResource	Addr;
		UDIInt32	PassCount;
		UDIBreakType	Type;
		REALPTR		BreakId;
	} params;

	if (BufferSizeCheck((FARPTR)(rm_address + sizeof(UDIBreakId)),"UDIPSetBreakpoint",PRINT_ON)) {
		return UDIErrorIPCLimitation;
	} /* if */

	/* Since only "out" parameters are passed by reference, don't
	 * need to copy anything into the parameter passing buffer before
	 * the call.  
	 */
	params.Addr      = Addr;
	params.PassCount = PassCount;
	params.Type      = Type;
	params.BreakId   = rm_buffer_addr;

	Err = REALCALL(UDISetBreakpointAddr,params);

	CheckRealError(Err,"UDISetBreakpoint");

	/* Need to copy "out" parameter data back into protected mode
	 * address space. 
	 */
	_fmemmove(NEARPTR_TO_FARPTR(BreakId),rm_address,sizeof(UDIBreakId));

	return FUNC_VAL;

} /* UDIPSetBreakpoint() */



UDIError UDIPQueryBreakpoint(
	REALPTR		UDIQueryBreakpointAddr,
	UDIBreakId	BreakId,		/* In  */
	UDIResource	*Addr,			/* Out */
	UDIInt32	*PassCount,		/* Out */
	UDIBreakType	*Type,			/* Out */
	UDIInt32	*CurrentCount		/* Out */
	)
{
	UDIError		Err;
	UDIInt32 _far		*PassCountPtr;
	UDIBreakType _far	*TypePtr;
	UDIInt32 _far		*CurrentCountPtr;

	_Packed struct {
		UDIBreakId	BreakId;
		REALPTR		Addr;
		REALPTR		PassCount;
		REALPTR		Type;
		REALPTR		CurrentCount;
	} params;

	/* Since all passed-by-reference variables are "out", don't need
	 * to copy data to parameter passing buffer.  Do need to set up
	 * pointers for real-mode function to use though.
	 */
	PassCountPtr    = (UDIInt32 _far *) (rm_address + sizeof(UDIResource));
	TypePtr         = (UDIBreakType _far *) (PassCountPtr + sizeof(UDIInt32));
	CurrentCountPtr = (UDIInt32 _far *) (TypePtr + sizeof(UDIBreakType));

	if (BufferSizeCheck((FARPTR)(CurrentCountPtr + sizeof(UDIBreakType)),"UDIPQueryBreakpoint",PRINT_ON)) {
		return UDIErrorIPCLimitation;
	} /* if */

	params.BreakId      = BreakId;
	params.Addr         = rm_buffer_addr;
	params.PassCount    = PROT_TO_REAL((FARPTR)PassCountPtr);
	params.Type         = PROT_TO_REAL((FARPTR)TypePtr);
	params.CurrentCount = PROT_TO_REAL((FARPTR)CurrentCountPtr);

	Err = REALCALL(UDIQueryBreakpointAddr,params);

	CheckRealError(Err,"UDIQueryBreakpoint");

	/* Copy returned values back into protected mode variables. */
	_fmemmove(NEARPTR_TO_FARPTR(Addr),rm_address,sizeof(UDIResource));
	_fmemmove(NEARPTR_TO_FARPTR(PassCount),PassCountPtr,sizeof(UDIInt32));
	_fmemmove(NEARPTR_TO_FARPTR(Type),TypePtr,sizeof(UDIBreakType));
	_fmemmove(NEARPTR_TO_FARPTR(CurrentCount),CurrentCountPtr,sizeof(UDIInt32));

	return FUNC_VAL;

} /* UDIPQueryBreakpoint() */



UDIError UDIPClearBreakpoint(
	REALPTR		UDIClearBreakpointAddr,
	UDIBreakId	BreakId			/* In  */
	)
{
	UDIError	Err;

	/* Don't need packed structure since only one parameter is passed. */

	Err = REALCALL(UDIClearBreakpointAddr,BreakId);

	CheckRealError(Err,"UDIClearBreakpoint");

	return FUNC_VAL;

} /* UDIPClearBreakpoint() */




UDIError UDIPGetStdout(
	REALPTR		UDIGetStdoutAddr,
	UDIHostMemPtr	Buf,			/* Out */
	UDISizeT	BufSize,		/* In  */
	UDISizeT	*CountDone		/* Out */
	)
{
	UDIError	Err;
	UDISizeT _far	*CountDonePtr;

	char *		CurrentTo;
	UDISizeT	TotalDone = 0;
	UDISizeT	CurrentCount;
	UDISizeT	CurrentDone = 0;
	long		BufAdjust;

	_Packed struct {
		REALPTR		Buf;
		UDISizeT	BufSize;
		REALPTR		CountDone;
	} params;

	CurrentCount = BufSize;
	CurrentTo    = (char *) Buf;

	do {
		CountDonePtr = (UDISizeT _far *) (rm_address + CurrentCount);

		BufAdjust = BufferSizeCheck((FARPTR)(CountDonePtr + sizeof(UDISizeT)),"UDIPGetStdout",PRINT_ON);
		if (BufAdjust) {
			CurrentCount = (rm_end_address - rm_address - sizeof(UDISizeT));
			CountDonePtr = (UDISizeT _far * ) (rm_end_address - sizeof(UDISizeT));
		} /* if */

		params.Buf       = rm_buffer_addr;
		params.BufSize   = CurrentCount;
		params.CountDone = PROT_TO_REAL((FARPTR)CountDonePtr);

		Err = REALCALL(UDIGetStdoutAddr,params);

		CheckRealError(Err,"UDIGetStdout");

		/* Copy returned values back into protected mode variables. */
		_fmemmove(NEARPTR_TO_FARPTR(&CurrentDone),CountDonePtr,sizeof(UDISizeT));

		TotalDone += CurrentDone;

		if (CurrentDone <= CurrentCount) 
			_fmemmove(NEARPTR_TO_FARPTR(CurrentTo),rm_address,CurrentDone);
		else {
			_fmemmove(NEARPTR_TO_FARPTR(CurrentTo),rm_address,CurrentDone);
			SIZE_ERROR(*CountDone, BufSize, "UDIPGetStdout");
		}

		/* Update looping variables for possible next pass */
		CurrentTo += CurrentDone;


	} while ((TotalDone < BufSize) & (CurrentDone == CurrentCount) 
		& (FUNC_VAL == UDINoError));

	*CountDone = TotalDone;

	return FUNC_VAL;

} /* UDIPGetStdout() */



UDIError UDIPGetStderr(
	REALPTR		UDIGetStderrAddr,
	UDIHostMemPtr	Buf,			/* Out */
	UDISizeT	BufSize,		/* In  */
	UDISizeT	*CountDone		/* Out */
	)
{
	UDIError	Err;
	UDISizeT _far	*CountDonePtr;
	long		Buffer_Adjustment;

	_Packed struct {
		REALPTR		Buf;
		UDISizeT	BufSize;
		REALPTR		CountDone;
	} params;

	/* Since all passed-by-reference variables are "out", don't need
	 * to copy data to parameter passing buffer.  Do need to set up
	 * pointers for real-mode function to use though.
	 */
	CountDonePtr = (UDISizeT _far *) (rm_address + BufSize);

	if ((Buffer_Adjustment = BufferSizeCheck((FARPTR)(CountDonePtr + sizeof(UDISizeT)),"UDIPGetStderr",PRINT_ON))) {
		if (BufSize <= Buffer_Adjustment)
			return UDIErrorIPCLimitation;
		BufSize -= Buffer_Adjustment;
	} /* if */

	params.Buf       = rm_buffer_addr;
	params.BufSize   = BufSize;
	params.CountDone = PROT_TO_REAL((FARPTR)CountDonePtr);

	Err = REALCALL(UDIGetStderrAddr,params);

	CheckRealError(Err,"UDIGetStderr");

	/* Copy returned values back into protected mode variables. */
	_fmemmove(NEARPTR_TO_FARPTR(CountDone),CountDonePtr,sizeof(UDISizeT));
	if (*CountDone <= BufSize)
		_fmemmove(NEARPTR_TO_FARPTR(Buf),rm_address,*CountDone);
	else {
		_fmemmove(NEARPTR_TO_FARPTR(Buf),rm_address,BufSize);
		SIZE_ERROR(*CountDone, BufSize, "UDIPGetStderr");
	}

	return FUNC_VAL;
	
} /* UDIPGetStderr() */



UDIError UDIPPutStdin(
	REALPTR		UDIPutStdinAddr,
	UDIHostMemPtr	Buf,			/* In  */
	UDISizeT	Count,			/* In  */
	UDISizeT	*CountDone		/* Out */
	)
{
	UDIError	Err;
	UDISizeT _far	*CountDonePtr;
	long		Buffer_Adjustment;

	_Packed struct {
		REALPTR		Buf;
		UDISizeT	Count;
		REALPTR		CountDone;
	} params;

	/* Get pointers to passed-by-reference variables in parameter
	 * passing buffer. 
	 */
	CountDonePtr = (UDISizeT _far *) (rm_address + Count);

	if ((Buffer_Adjustment = BufferSizeCheck((FARPTR)(CountDonePtr + sizeof(UDISizeT)),"UDIPPutStdin",PRINT_ON))) {
		if (Count <= Buffer_Adjustment)
			return UDIErrorIPCLimitation;
		Count -= Buffer_Adjustment;
	} /* if */

	/* Copy "in" passed-by-reference variables to parameter passing
	 * buffer.
	 */
	_fmemmove(rm_address,NEARPTR_TO_FARPTR(Buf),Count);

	params.Buf       = rm_buffer_addr;
	params.Count     = Count;
	params.CountDone = PROT_TO_REAL((FARPTR)CountDonePtr);

	Err = REALCALL(UDIPutStdinAddr,params);

	CheckRealError(Err,"UDIPutStdin");

	/* Copy returned values back into protected mode variables. */
	_fmemmove(NEARPTR_TO_FARPTR(CountDone),CountDonePtr,sizeof(UDISizeT));

	return FUNC_VAL;


} /* UDIPPutStdin() */



UDIError UDIPStdinMode(
	REALPTR		UDIStdinModeAddr,
	UDIMode		*Mode			/* Out */
	)
{
	UDIError	Err;

	/* Don't need to copy anything in parameter passing area since there
	 * are no "in", passed-by-reference parameters.  
	 * Since there is only a single parameter and that is "out", and
	 * passed-by-reference, can just use the address of the parameter
	 * passing buffer for this.
	 */
	Err = REALCALL(UDIStdinModeAddr,rm_buffer_addr);

	CheckRealError(Err,"UDIStdinMode");

	_fmemmove(NEARPTR_TO_FARPTR(Mode),rm_address,sizeof(UDIMode));

	return FUNC_VAL;

} /* UDIPStdinMode() */



UDIError UDIPPutTrans(
	REALPTR		UDIPutTransAddr,
	UDIHostMemPtr	Buf,			/* In  */
	UDISizeT	Count,			/* In  */
	UDISizeT	*CountDone		/* Out */
	)
{
	UDIError	Err;
	UDISizeT _far	*CountDonePtr;

	char *		CurrentTo;
	UDISizeT	TotalDone=0;
	UDISizeT	CurrentCount;
	UDISizeT	CurrentDone=0;
	long		BufAdjust;

	_Packed struct {
		REALPTR		Buf;
		UDISizeT	Count;
		REALPTR		CountDone;
	} params;

	CurrentCount = Count;
	CurrentTo = Buf;

	do {
		CountDonePtr = (UDISizeT _far *) (rm_address + CurrentCount);

		BufAdjust = BufferSizeCheck((FARPTR)(CountDonePtr + sizeof(UDISizeT)),"UDIPPutTrans",PRINT_OFF);
		if (BufAdjust) {
			CurrentCount = (rm_end_address - rm_address - sizeof(UDISizeT));
			CountDonePtr = (UDISizeT _far *) (rm_end_address - sizeof(UDISizeT));
		} /* if */

		_fmemmove(rm_address,NEARPTR_TO_FARPTR(Buf),Count);

		params.Buf       = rm_buffer_addr;
		params.Count     = CurrentCount;
		params.CountDone = PROT_TO_REAL((FARPTR)CountDonePtr);

		Err = REALCALL(UDIPutTransAddr,params);

		CheckRealError(Err,"UDIPutTrans");

		/* Copy the contents of the "out" parameters back into their
		 * protected mode counterparts.
		 */
		_fmemmove(NEARPTR_TO_FARPTR(&CurrentDone),CountDonePtr,sizeof(UDISizeT));

		TotalDone += CurrentDone;

	} while ((TotalDone < Count) & (FUNC_VAL == UDINoError));

	*CountDone = TotalDone;
	return FUNC_VAL;

} /* UDIPPutTrans() */



UDIError UDIPGetTrans(
	REALPTR		UDIGetTransAddr,
	UDIHostMemPtr	Buf,			/* Out */
	UDISizeT	BufSize,		/* In  */
	UDISizeT	*CountDone		/* Out */
	)
{
	UDIError	Err;
	UDISizeT _far	*CountDonePtr;
	long		BufAdjust;

	_Packed struct {
		REALPTR		Buf;
		UDISizeT	BufSize;
		REALPTR		CountDone;
	} params;

	/* Get pointers to passed-by-reference parameters. */
	CountDonePtr = (UDISizeT _far *) (rm_address + BufSize);

	if ((BufAdjust = BufferSizeCheck((FARPTR)(CountDonePtr + sizeof(UDISizeT)),"UDIPGetTrans",PRINT_ON))) {
		if (BufSize <= BufAdjust)
			return UDIErrorIPCLimitation;
		BufSize -= BufAdjust;
	} /* if */

	params.Buf       = rm_buffer_addr;
	params.BufSize   = BufSize;
	params.CountDone = PROT_TO_REAL((FARPTR)CountDonePtr);

	Err = REALCALL(UDIGetTransAddr,params);

	CheckRealError(Err,"UDIGetTrans");

	/* Copy the contents of the "out" parameters back into their
	 * protected mode counterparts.
	 */
	_fmemmove(NEARPTR_TO_FARPTR(CountDone),CountDonePtr,sizeof(UDISizeT));
	if (*CountDone <= BufSize)
		_fmemmove(NEARPTR_TO_FARPTR(Buf),rm_address,*CountDone);
	else {
		_fmemmove(NEARPTR_TO_FARPTR(Buf),rm_address,BufSize);
		SIZE_ERROR(*CountDone, BufSize, "UDIPGetTrans");
	}

	return FUNC_VAL;


} /* UDIPGetTrans() */


UDIError UDIPTransMode(
	REALPTR		UDITransModeAddr,
	UDIMode		*Mode			/* Out */
	)
{
	UDIError	Err;

	/* Don't need to copy anything in parameter passing area since there
	 * are no "in", passed-by-reference parameters.  
	 * Since there is only a single parameter and that is "out", and
	 * passed-by-reference, can just use the address of the parameter
	 * passing buffer for this.
	 */

	Err = REALCALL(UDITransModeAddr,rm_buffer_addr);

	CheckRealError(Err,"UDITransMode");

	_fmemmove(NEARPTR_TO_FARPTR(Mode),rm_address,sizeof(UDIMode));

	return FUNC_VAL;

} /* UDIPTransMode() */

