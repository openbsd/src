/******************************************************************************
* 
*       The process  and all routines contained  herein are the
*       property and trade secrets of AMD Inc. 
* 
*       Except as  provided  for by licence agreement, this code 
*       shall  not  be duplicated,  used or  disclosed  for  any 
*       purpose or reason, in whole or part, without the express 
*       written consent of AMD.
* 
*       Copyright  AMD Inc.  1991
*
*********************************************************************** MODULE
* 
*       $NAME     @(#)udip2iss.c	1.2 91/06/14
*       AUTHORS   UDI Team Members
* 
*       This module implements the UDI Procedural interface
*	for the ISS simulator.
********************************************************************** HISTORY
*
*
**************************************************************** INCLUDE FILES
*/
#include "stdio.h"
#include "udiproc.h"


/*********************************************************** UDI_GET_ERROR_MSG
     Errors above the value ERRUDI_TIP indicate that the
     TIP  was  not able to complete the request for some
     target   specific    reason.     The    DFE    uses
     UDIGetErrorMsg() to get the descriptive text for
     the error message which can then  be  displayed  to
     the user.
*/
UDIError UDIGetErrorMsg(error_code, msg)
UINT32          error_code;	/* in */
UDIHostMemPtr  msg;		/* out -- text of msg */
{
}

/*************************************************************** UDI_TERMINATE
     UDITerminate() is used to tell the  TIP  that  the
     DFE is finished.
*/
UDITerminate()
{
}

/******************************************************* UDI_GET_TARGET_CONFIG
     UDIGetTargetConfig() gets information about  the
     target.  I_mem_start/size defines the start address
     and   length    of    instruction    RAM    memory.
     D_mem_start/size  defines  the  start  address  and
     length     of     instruction     Data      memory.
     IR_mem_start/size  defines  the  start  address and
     length of instruction ROM memory.  coprocessor  de-
     fines the type of coprocessor present in the target
     if any.  max_breakpoints defines the maximum number
     of   breakpoints   which  the  target  can  handle.
     max_steps defines the maximum number  of  stepcount
     that can be used in the UDIStep command.
*/
UDIError UDIGetTargetConfig(I_mem_start, I_mem_size, D_mem_start,
		D_mem_size, R_mem_start, R_mem_size, cpu_prl, copro_prl)
UDIOffset  *I_mem_start;/* out */
UDIOffset  *I_mem_size;	/* out */
UDIOffset  *D_mem_start;/* out */
UDIOffset  *D_mem_size;	/* out */
UDIOffset  *R_mem_start;/* out */
UDIOffset  *R_mem_size;	/* out */
UINT32	   *cpu_prl;	/* out */
UINT32	   *copro_prl;	/* out */
{
}

/********************************************************** UDI_CREATE_PRCOESS
     UDICreateProcess() tells the  target  OS  that  a
     process is to be created and gets a pID back unless
     there is some error.
*/
UDIError UDICreateProcess(pid)
UDIPID	*pid;	/* out */
{
}

/********************************************************** UDI_SET_DEFALUT_PID
     UDISetDefaultPid  uses   a   pid   supplied   by
     UDICreateProcess  and  sets it as the default for all
     udi calls until a new default is set.  A user of  a
     single-process OS would only have to use this once.
*/
UDIError UDISetDefaultPid(pid)
UDIPID	pid;	/* in */
{
}

/********************************************************* UDI_DESTROY_PROCESS
     UDIDestroyProcess() frees a process resource pre-
     viously created by UDICreateProcess().
*/
UDIError UDIDestroyProcess(pid)
UDIPID   pid;	/* in */
{
}

/****************************************************** UDI_INITIALIZE_PROCESS
     UDIInitializeProcess() is called after  the  code
     for a process has been loaded.  The pid used is the
     one  set  by  UDISetDfaultPid.   The  parameter
     text_addr  defines  the lowest and highest text ad-
     dresses  used  by  the  process.    The   parameter
     data_addr  defines  the lowest and highest data ad-
     dresses  used  by  the   process.    The   paramter
     entry_point defines the entry point of the process.
     The parameters mem_stack_size  and  reg_stack  size
     define  the sizes of the memory and register stacks
     required  by  the  process.   The   special   value
     UDI_DEFAULT  implies  that  the default stack sizes
     for the target OS should be  used.   The  parameter
     argstring  defines a character string that will get
     parsed into the argv array for  the  process.   The
     target  OS will use the supplied information to set
     up the heaps and stacks and the  program  arguments
     if any.  On return; the PC will be set to the entry
     point of the process.
*/
UDIError  UDIInitializeProcess( text_addr, data_addr, entry_point,
			mem_stack_size, reg_stack_size, argstring)
UDIRange    text_addr;		/* in--lowest and highest text addrs */
UDIRange    data_addr;		/* in--lowest and highest data addrs */
UDIResource entry_point;	/* in--process entry point */
CPUSizeT    mem_stack_size;	/* in--memory stack size */
CPUSizeT    reg_stack_size;	/* in--register stack size */
char*	    argstring;	/* in--argument string used to */
{
}

/****************************************************************** UDI_READ
     UDIRead() reads a block of objects from  a  target
     address+space  to host space.  The parameter struc-
     ture "from" specifies the address space and  offset
     of  the  source.   The parameter "to" specifies the
     destination address in the DFE on  the  host.   The
     parameter  count specifies the number of objects to
     be transferred and "size"  specifies  the  size  of
     each  object.
     The size parameter is used by the TIP to
     perform byte-swapping if the target is not the same
     endian as the  host.   On  completion;  the  output
     parameter  count_done  is  set to the number of ob-
     jects successfully transferred.
*/

UDIError UDIRead (from, to, count, size, count_done, host_endian)
UDIResource	from;		/* in - source address on target */
UDIVoidPtr	to;		/* out - destination address on host */
UDICount	count;		/* in -- count of objects to be transferred */
UDISize		size;		/* in -- size of each object */
UDICount	*count_done;	/* out - count actually transferred */
UDIBool		host_endian;	/* in -- flag for endian information */
{
}

/****************************************************************** UDI_WRITE
     UDIWrite() writes a block  of  objects  from  host
     space  to  a  target  address+space  The  parameter
     "from" specifies the source address in the  DFE  on
     the  host.   The parameter structure "to" specifies
     the address space and offset of the destination  on
     the  target.   The  parameter  count  specifies the
     number of objects  to  be  transferred  and  "size"
     specifies the size of each object. The size parameter
     is used by the TIP to perform byte-swapping if
     the target is not the same endian as the host.   On
     completion;  the output parameter count_done is set
     to the number of objects successfully transferred.
*/
UDIError UDIWrite( from, to, count, size, count_done, HostEndian )
UDIResource	from;		/* in -- destination address on host */
UDIResource	to;		/* in -- source address on target */
UDICount	count;		/* in -- count of objects to be transferred */
UDISize		size;		/* in -- size of each object */
UDICount	*count_done;	/* out - count actually transferred */
UDIBool		HostEndian;	/* in -- flag for endian information */
{
}

/******************************************************************** UDI_COPY
     UDICopy() copies a block of objects from one  tar-
     get  address/space to another target address/space.
     If the source and destination overlap; the copy  is
     implemented as if a temporary buffer was used.  The
     parameter structure "from"  specifies  the  address
     space  and offset of the destination on the target.
     The parameter structure "to" specifies the  address
     space  and offset of the destination on the target.
     The parameter count specifies the number of objects
     to  be transferred and "size" specifies the size of
     each object.  On completion; the  output  parameter
     count_done is set to the number of objects success-
     fully transferred.
*/
UDIError UDICopy(from, to, count, size, count_done, direction )
UDIResource	from;		/* in -- destination address on target */
UDIResource	to;		/* in -- source address on target */
UDICount	count;		/* in -- count of objects to be transferred */
UDISize		size;		/* in -- size of each object */
UDICount	*count_done;	/* out - count actually transferred */
UDIBool		direction;	/* in -- high-to-low or reverse */
{
}

/***************************************************************** UDI_EXECUTE
     UDIExecute() continues execution  of  the  default
     process from the current PC.
*/
UDIError UDIExecute()
{
}

/******************************************************************** UDI_STEP
     UDIStep()  specifies  a  number  of  "instruction"
     steps  to  make.  The step can be further qualified
     to state whether CALLs  should  or  should  not  be
     stepped over; whether TRAPs should or should not be
     stepped over; and whether stepping should halt when
     the PC gets outside a certain range.  The semantics
     of UDIStep imply that progress  is  made;  ie;  at
     least  one  instruction is executed before traps or
     interrupts are handled.
*/
UDIError UDIStep(steps, steptype, range)
UINT32	      steps;          /* in -- number of steps */
UDIStepType   steptype;       /* in -- type of stepping to be done */
UDIRange      range;          /* in -- range if StepInRange is TRUE */
{
}

/******************************************************************** UDI_STOP
     UDIStop() stops the default process
*/
UDIError UDIStop(stop_pc)
UDIResource	*stop_pc;	/* out -- value of PC where we stopped */
{
}

/******************************************************************** UDI_WAIT
     UDIWait() returns the state of the target  proces-
     sor.  The TIP is expected to return when the target
     state is no longer RUNNING  or  when  maxtime  mil-
     liseconds have elapsed; whichever comes first.  The
     special maxtime value UDI_WAIT_FOREVER  essentially
     means  that  the  DFE blocks until the target is no
     longer RUNNING.  On completion; pid is used to  re-
     port  which  process  stopped (necessary for multi-
     process targets).  On completion; stop_pc is usual-
     ly set to the PC where execution stopped.

     The return status STDIN_NEEDED allows  the  TIP  to
     tell  the DFE that the target program is requesting
     input  and  the  TIP's  own  internal   buffer   of
     charcters is empty.  The DFE can inform the user of
     this situation if it desires.

     Possible states are:
	     NOT EXECUTING
             RUNNING
             STOPPED (due to UDIStop)
             BREAK   (breakpoint hit)
             STEPPED (completed number of steps requested by UDIStep)
             WAITING (wait mode bit set)
             HALTED  (at a halt instruction)
             WARNED  (not executing because WARN line asserted)
             TRAPPED (invalid trap taken; indicates trap number)
             STDOUT_READY (stopped waiting for stdout to be output)
             STDERR_READY (stopped waiting for stderr to be output)
             STDIN_NEEDED (stopped waiting for stdin to be supplied)
*/
UDIError UDIWait(maxtime, pid, stop_reason)
INT32      maxtime;        /* in -- maximum time to wait for completion */
UDIPID     *pid;           /* out -- pid of process which stopped if any */
UINT32     *stop_reason;   /* out -- PC where process stopped */
{
}

/********************************************************** UDI_SET_BREAKPOINT
     UDISetBreakpoint() sets a breakpoint  at  an  ad-
     dress  and  uses  the  passcount  to state how many
     times that instruction should  be  hit  before  the
     break  occurs.   The  passcount  continues to count
     down; even if a different breakpoint is hit and  is
     reinitialized  only when this breakpoint is hit.  A
     passcount value of 0 indicates a non-sticky  break-
     point  that  will  be  removed  whenever  execution
     stops.
*/
UDIError UDISetBreakpoint (addr, passcount, type)
UDIResource	addr;		/* in -- where breakpoint gets set */
INT32		passcount;	/* in -- passcount for breakpoint  */
UDIBreakType	type;		/* in -- breakpoint type */
{
}

/******************************************************** UDI_QUERY_BREAKPOINT
*/
UDIError UDIQueryBreakpoint (addr, count)
UDIResource	addr;		/* in -- where breakpoint gets set */
UINT32		*count;		/* out - passcount for breakpoint  */
{
}

/******************************************************** UDI_CLEAR_BREAKPOINT
     UDIClearBreakpoint() is used to  clear  a  break-
     point.
*/
UDIError UDIClearBreakpoint (addr)
UDIResource	addr;	/* in -- which breakpoint to clear */
{
}

/************************************************************** UDI_GET_STDOUT
     UDIGetStdout()  is  called   when   a   call   to
     UDIWait()  returns  with  the status STDOUT_READY.
     The parameter "buf" specifies the DFE's buffer  ad-
     dress  which  is  expected to be filled by the TIP.
     The parameter "bufsize" specifies the size of  this
     buffer.  On return; count_done is set to the number
     of bytes actually written to buf.  The  DFE  should
     keep  calling  UDIGetStdout() until count_done is
     less than bufsize.
*/
UDIError UDIGetStdout(buf, bufsize, count_done)
UDIHostMemPtr	buf;		/* out -- buffer to be filled */
CPUSizeT	bufsize;	/* in -- buffer size in bytes */
CPUSizeT	*count_done;	/* out -- number of bytes written to buf */
{
}

/************************************************************** UDI_GET_STDERR
     UDIGetStderr()  is  called   when   a   call   to
     UDIWait()  returns  with  the status STDERR_READY.
     In   other    respects    it    is    similar    to
     UDIGetStdout().
*/
UDIError UDIGetStderr(buf, bufsize, count)
UDIHostMemPtr buf;	/* out -- buffer to be filled */
UINT32	bufsize;	/* in  -- buffer size in bytes */
INT32	*count;		/* out -- number of bytes written to buf */
{
}

/*************************************************************** UDI_PUT_STDIN
     UDIPutStdin() is called whenever the DFE wants to
     deliver an input character to the TIP.  This may be
     in response to a status STDIN_NEEDED but  need  not
     be.   (Some  target  operating  systems  will never
     block for input).  Any buffering and  line  editing
     of  the  stdin  characters is done under control of
     the TIP.
*/
INT32	UDIPutStdin (buf, bufsize, count)
UDIHostMemPtr buf;	/* out - buffer to be filled */
UINT32	bufsize;	/* in -- buffer size in bytes */
INT32	*count;		/* out - number of bytes written to buf */
{
}

/*************************************************************** UDI_PUT_TRANS
     UDIPutTrans() is used to feed input to  the  pass-
     thru  mode.   The  parameter "buf" is points to the
     input data in DFE memory.   The  parameter  "count"
     specifies the number of bytes.
*/
INT32	UDIPutTrans (buf, count)
UDIHostMemPtr	buf;	/* in -- buffer address containing input data */
CPUSizeT	count;	/* in -- number of bytes in buf */
{
}

/*************************************************************** UDI_GET_TRANS
     UDIGetTrans() is used to get output lines from the
     pass-thru mode The parameter "buf" specifies to the
     buffer to be filled in DFE space.  "bufsize" speci-
     fies the size of the buffer and; on return, "count"
     is set to the number of bytes put  in  the  buffer.
     The DFE should continue to call UDIGetTrans() un-
     til count is less than bufsize.  Other possible re-
     turn values are:
             EOF -- leave transparent mode
             UDI_GET_INPUT -- host should get  some  in-
     put;                                  then     call
     UDIPutTrans().
*/
INT32	UDIGetTrans (buf, bufsize, count)
UDIHostMemPtr	buf;		/* out -- buffer to be filled */
CPUSizeT	bufsize;	/* in  -- size of buf */
CPUSizeT	*count;		/* out -- number of bytes in buf */
{
}
