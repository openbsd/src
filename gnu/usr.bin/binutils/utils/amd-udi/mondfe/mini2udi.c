static char _[] = "@(#)mini2udi.c	5.23 93/08/18 13:48:08, Srini, AMD. ";
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
 *      Engineer:  Srini Subramanian.
 *****************************************************************************
 * Definitions of the functions that define Minimon's Interface 
 * to the UDI
 * interface The minimon functions are declared in miniint.h The UDI
 * functions are declared in udi/udiproc.h
 *****************************************************************************
 */


#include <stdio.h>
#ifdef	MSDOS
#include <io.h>
#endif
#include <string.h>
#include "main.h"
#include "memspcs.h"
#include "macros.h"
#include "miniint.h"
#include "udiproc.h"
#include "udiids.h"
#include "udiext.h"
#include "versions.h"


/* Define BreakIdType here to avoid having to change at many places
 * every time it changes.
 */
typedef	unsigned int	BreakIdType;

/* ABOUT UDI calls:
 * There are three types of return values:
 * < 0: means a TIP failure.
 * = 0: means success.
 * > 0: means a "local" failure.
 */



static	UDISessionId	SessionID;
static	char            MONErrorMsg[MONErrorMsgSize];
static	int		GoForever=0;

static	char	*udi_errmsg[] = {
/*
#define UDINoError			0
*/ "UDIERR: No Error",
/*
#define UDIErrorNoSuchConfiguration	1
*/ "UDIERR: No Such Configuration in Config File.",
/*
#define UDIErrorCantHappen		2
*/ "UDIERR: Cannot Happen With Current Environment Setup.",
/*
#define UDIErrorCantConnect		3
*/ "UDIERR: Cannot Connect to TIP Specified.",
/*
#define UDIErrorNoSuchConnection	4
*/ "UDIERR: No Such Connection Found.",
/*
#define UDIErrorNoConnection		5
*/ "UDIERR: No Connection Occurred.",
/*
#define UDIErrorCantOpenConfigFile	6
*/ "UDIERR: Cannot Open UDI Config File.",
/*
#define UDIErrorCantStartTIP		7
*/ "UDIERR: Cannot Start TIP In Current Environment Setup.",
/*
#define UDIErrorConnectionUnavailable	8
*/ "UDIERR: Requested Connection Unavailable.",
/*
#define UDIErrorTryAnotherTIP		9
*/ "UDIERR: Try Another TIP For Connection.",
/*
#define UDIErrorExecutableNotTIP	10
*/ "UDIERR: TIP Specified in Config File Not An Executable.",
/*
#define UDIErrorInvalidTIPOption	11
*/ "UDIERR: Connection Failed Due To Invalid TIP Options in Config File.",
/*
#define UDIErrorCantDisconnect		12
*/ "UDIERR: Cannot Disconnect TIP",
/*
#define UDIErrorUnknownError		13
*/ "UDIERR: Unknown Error Number Specified.",
/*
#define UDIErrorCantCreateProcess	14
*/ "UDIERR: TIP Cannot Create a New Process.",
/*
#define UDIErrorNoSuchProcess		15
*/ "UDIERR: No Such Process in the Current TIP.",
/*
#define UDIErrorUnknownResourceSpace	16
*/ "UDIERR: Unknown Resource Space Encountered By TIP.",
/*
#define UDIErrorInvalidResource		17
*/ "UDIERR: Invalid Resource Specified To TIP.",
/*
#define UDIErrorUnsupportedStepType	18
*/ "UDIERR: Unsupported Step Type For This TIP Specified.",
/*
#define UDIErrorCantSetBreakpoint	19
*/ "UDIERR: Could Not Set The Breakpoint.",
/*
#define UDIErrorTooManyBreakpoints	20
*/ "UDIERR: Too Many Breakpoints Already In Use.",
/*
#define UDIErrorInvalidBreakId		21
*/ "UDIERR: Breakpoint Does Not Exist For This BreakId.",
/*
#define UDIErrorNoMoreBreakIds		22
*/ "UDIERR: No More Breakpoints. BreakId Too High.",
/*
#define UDIErrorUnsupportedService	23
*/ "UDIERR: TIP Does Not Support The Requested Service.",
/*
#define UDIErrorTryAgain		24
*/ "UDIERR: Error Occurred. Trying Again.",
/*
#define UDIErrorIPCLimitation		25
*/ "UDIERR: IPC Limitation Exceeded.",
/*
#define UDIErrorIncomplete		26
*/ "UDIERR: Service Incomplete.More Data Available.",
/*
#define UDIErrorAborted			27
*/ "UDIERR: Aborted Requested Service.",
/*
#define UDIErrorTransDone		28
*/ "UDIERR: Transaction Completed.",
/*
#define UDIErrorCantAccept		29
*/ "UDIERR: Cannot Accept.",
/*
#define UDIErrorTransInputNeeded	30
*/ "UDIERR: Transaction Input Needed.",
/*
#define UDIErrorTransModeX		31
*/ "UDIERR: Transaction ModeX",
/*
#define UDIErrorInvalidSize		32
*/ "UDIERR: Invalid Object Size Specified.",
/*
#define UDIErrorBadConfigFileEntry	33
*/ "UDIERR: Bad Entry In UDI Config File Found.",
/*
#define UDIErrorIPCInternal		34
*/ "UDIERR: Internal Error Occurred In IPC Layer."
};

static	UDIPId		CurrentPID=(UDIPId) UDIProcessProcessor;
static	void		PrintErrorMessage PARAMS((UDIError num));
static	void		udi_warning PARAMS((int num));
static	CPUSpace	xlate_mspace_mon2udi PARAMS((INT32 mspace));
static	INT32		xlate_mspace_udi2mon PARAMS((CPUSpace mspace));
static UDIError 	FillString PARAMS(( UDIResource from, 
					   UDIHostMemPtr pattern, 
					   UDISizeT   pattern_count,
					   UDICount   fill_count));
static UDIError 	FillWords PARAMS(( UDIResource from, 
					   UDIHostMemPtr pattern, 
					   UDISizeT   pattern_count,
					   UDICount   fill_count));


INT32
/*********************************************Mini_TIP_init    */
Mini_TIP_init(connect_string, mon_session_id)
  char           *connect_string;
  int		 *mon_session_id;
{
  UDIError	UDIretval;
  UDISessionId	session;

  /* First connect the target  */
  if ((UDIretval = UDIConnect(connect_string,
			   &session)) <= TIPFAILURE) {
    SessionID = session; 
    *mon_session_id = (int) session;
    PrintErrorMessage (UDIretval);
    return ((INT32) UDIretval);
  } else if (UDIretval == SUCCESS) {
     SessionID = session;
    *mon_session_id = (int) session;
     return (SUCCESS);
  } else {
     SessionID = session;
    *mon_session_id = (int) session;
     udi_warning(UDIretval);
     return((INT32) UDIretval);
  };
}

INT32
Mini_TIP_Capabilities()
{
  UDIError	UDIretval;
  UDIUInt32	TIPId;			/* Out */
  UDIUInt32	TargetId;		/* Out */
  UDIUInt32	DFEId;			/* In */
  UDIUInt32	DFE;			/* In */
  UDIUInt32	TIP;			/* Out */
  UDIUInt32	DFEIPCId;		/* Out */
  UDIUInt32	TIPIPCId;		/* Out */
  char		TIPString[80];		/* Out */

    (void) strcpy (TIPString,"");
    DFEId = (UDIUInt32) UDIID (UDIProductCode_Mondfe, MONDFERev, MONDFESubRev, MONDFESubSubRev);
    DFE = (UDIUInt32) MONDFEUDIVers;
    if ((UDIretval = UDICapabilities ( &TIPId, 
				       &TargetId,
				       DFEId,
				       DFE,
				       &TIP,
				       &DFEIPCId,
				       &TIPIPCId,
				       &TIPString[0])) <= TIPFAILURE) {
       PrintErrorMessage (UDIretval);
       return (FAILURE);
     } else if (UDIretval == SUCCESS) {
       if (!QuietMode) {
	 if (io_config.echo_mode == (INT32) TRUE) {
	   fprintf(io_config.echo_file, "MiniMON29K Release 3.0\n");
           fprintf(io_config.echo_file, ">AMD MONDFE Version: %d.%d.%d",
				  (int) ((DFEId & 0x00000F00) >> 8),
				  (int) ((DFEId & 0x000000F0) >> 4),
				  (int) ((DFEId & 0x0000000F) >> 0));
           fprintf(io_config.echo_file, "\tIPC Version: %d.%d.%d UDI Rev. %d.%d.%d <\n",
				  (int) ((DFEIPCId & 0x00000F00) >> 8),
				  (int) ((DFEIPCId & 0x000000F0) >> 4),
				  (int) ((DFEIPCId & 0x0000000F) >> 0),
				  (int) ((DFE & 0x00000F00) >> 8),
				  (int) ((DFE & 0x000000F0) >> 4),
				  (int) ((DFE & 0x0000000F) >> 0));
           fprintf(io_config.echo_file, "%s\n", TIPString);
           fprintf(io_config.echo_file, ">TIP Version: %d.%d.%d",
				  (int) ((TIPId & 0x00000F00) >> 8),
				  (int) ((TIPId & 0x000000F0) >> 4),
				  (int) ((TIPId & 0x0000000F) >> 0));
           fprintf(io_config.echo_file, "\tIPC Version: %d.%d.%d UDI Rev. %d.%d.%d<\n",
				  (int) ((TIPIPCId & 0x00000F00) >> 8),
				  (int) ((TIPIPCId & 0x000000F0) >> 4),
				  (int) ((TIPIPCId & 0x0000000F) >> 0),
				  (int) ((TIP & 0x00000F00) >> 8),
				  (int) ((TIP & 0x000000F0) >> 4),
				  (int) ((TIP & 0x0000000F) >> 0));
	 }
	 fprintf(stderr, "MiniMON29K Release 3.0\n");
         fprintf(stderr, ">AMD MONDFE Version: %d.%d.%d",
				  (int) ((DFEId & 0x00000F00) >> 8),
				  (int) ((DFEId & 0x000000F0) >> 4),
				  (int) ((DFEId & 0x0000000F) >> 0));
         fprintf(stderr, "\tIPC Version: %d.%d.%d UDI Rev. %d.%d.%d <\n",
				  (int) ((DFEIPCId & 0x00000F00) >> 8),
				  (int) ((DFEIPCId & 0x000000F0) >> 4),
				  (int) ((DFEIPCId & 0x0000000F) >> 0),
				  (int) ((DFE & 0x00000F00) >> 8),
				  (int) ((DFE & 0x000000F0) >> 4),
				  (int) ((DFE & 0x0000000F) >> 0));
         fprintf(stderr, "%s\n", TIPString);
         fprintf(stderr, ">TIP Version: %d.%d.%d",
				  (int) ((TIPId & 0x00000F00) >> 8),
				  (int) ((TIPId & 0x000000F0) >> 4),
				  (int) ((TIPId & 0x0000000F) >> 0));
         fprintf(stderr, "\tIPC Version: %d.%d.%d UDI Rev. %d.%d.%d<\n",
				  (int) ((TIPIPCId & 0x00000F00) >> 8),
				  (int) ((TIPIPCId & 0x000000F0) >> 4),
				  (int) ((TIPIPCId & 0x0000000F) >> 0),
				  (int) ((TIP & 0x00000F00) >> 8),
				  (int) ((TIP & 0x000000F0) >> 4),
				  (int) ((TIP & 0x0000000F) >> 0));
       }
       if ( (int) ((TIPId & 0x00000F00) >> 8) <
	    (int) ((DFEId & 0x00000F00) >> 8) ) {
	 fprintf(stderr, "!!!!! WARNING: MONTIP's major version number is older than that of MONDFE's         !!!!!\n");
	 fprintf(stderr, "!!!!! Please verify the versions and call AMD 29K Technical Support for assistance. !!!!!\n");
       }
       if ((TIP == (UDIUInt32) 0) || ((TIP & 0xFFF) > (DFE & 0xFFF))) {
	  fprintf(stderr, "UDI WARNING: UDI Versions NOT Compatible.\n");
       }
       if (TIP == (UDIUInt32) 0)
	  return (FAILURE);
       return (SUCCESS);
     } else {
        udi_warning(UDIretval);
        return(FAILURE);
     }
}

INT32
Mini_TIP_CreateProc()
{
     UDIError	UDIretval;
     UDIPId	pid;

     if ((UDIretval = UDICreateProcess(&pid)) <= TIPFAILURE) {
       PrintErrorMessage (UDIretval);
       return (FAILURE);
     } else if (UDIretval == SUCCESS) {
       CurrentPID = pid;
       return (SUCCESS);
     } else {
        udi_warning(UDIretval);
        return(FAILURE);
     }
}

INT32
Mini_TIP_disc()
{
  UDIError	UDIretval;

  if ((UDIretval = UDIDisconnect(SessionID, 
			      UDIContinueSession)) <= TIPFAILURE) {
       PrintErrorMessage (UDIretval);
       return (FAILURE);
  } else if (UDIretval == SUCCESS) {
       return(SUCCESS);
  } else {
        udi_warning(UDIretval);
        return(FAILURE);
  };
}

INT32
Mini_TIP_SetCurrSession(sid)
int	sid;
{
  UDIError	UDIretval;

  if ((UDIretval = UDISetCurrentConnection((UDISessionId) sid)) <= TIPFAILURE) {
       PrintErrorMessage (UDIretval);
       return (FAILURE);
  } else if (UDIretval == SUCCESS) {
       SessionID = (UDISessionId) sid;
       return (SUCCESS);
  } else {
        udi_warning(UDIretval);
        return(FAILURE);
  }
}

INT32
Mini_TIP_SetPID(pid)
int	pid;
{
  UDIError	UDIretval;

  if ((UDIretval = UDISetCurrentProcess((UDIPId) pid)) <= TIPFAILURE) {
       PrintErrorMessage (UDIretval);
       return (FAILURE);
  } else if (UDIretval == SUCCESS) {
       return (SUCCESS);
  } else {
        udi_warning(UDIretval);
        return(FAILURE);
  }
}

INT32
Mini_TIP_DestroyProc()
{
  UDIError	UDIretval;

  if ((UDIretval = UDIDestroyProcess(CurrentPID)) <= TIPFAILURE) {
       PrintErrorMessage (UDIretval);
       return (FAILURE);
  } else if (UDIretval == SUCCESS) {
       return (SUCCESS);
  } else {
        udi_warning(UDIretval);
        return(FAILURE);
  }
}

INT32
Mini_TIP_exit()
{
  UDIError	UDIretval;

     if ((UDIretval = UDIDisconnect(SessionID, 
			      UDITerminateSession)) <= TIPFAILURE) {
       PrintErrorMessage (UDIretval);
       return (FAILURE);
     } else if (UDIretval == SUCCESS) {
       return(SUCCESS);
     } else {
        udi_warning(UDIretval);
        return(FAILURE);
     };
}
/* Breakpoint routines    */

/* Remove breakpoint      */
INT32
/*******************************************Mini_bkpt_rm   */
Mini_bkpt_rm(break_id)
int       break_id;
{
  UDIError	UDIretval;

  if ((UDIretval = UDIClearBreakpoint ((BreakIdType) break_id)) <= TIPFAILURE) {
    PrintErrorMessage (UDIretval);
    return (FAILURE);
  } else if (UDIretval == SUCCESS) {
    return(SUCCESS);
  } else {
     udi_warning(UDIretval);
     return(FAILURE);
  };
}

/* Set   Breakpoints    */

INT32
/**********************************************Mini_bkpt_set   */
Mini_bkpt_set(m_space, address, pass_count, type, break_id)
  INT32        m_space;
  ADDR32          address;
  INT32        pass_count;
  INT32        type;
  int       *break_id;
{
  UDIResource	addr;
  UDIError	UDIretval;


  addr.Space = xlate_mspace_mon2udi(m_space);
  addr.Offset = address;

  if (type == BKPT_29000)
     type = (UDIBreakType) UDIBreakFlagExecute;
  else if (type == BKPT_29050)
     type = (UDIBreakType) (MONBreakFlagHardware | UDIBreakFlagExecute);
  else if (type == BKPT_29050_BTE_0)
     type = (UDIBreakType) (MONBreakFlagHardware | UDIBreakFlagExecute);
  else if (type == BKPT_29050_BTE_1)
     type = (UDIBreakType) (MONBreakTranslationEnabled | MONBreakFlagHardware | UDIBreakFlagExecute);

  if ((UDIretval = UDISetBreakpoint(addr,
			   (UDIInt32) pass_count,
			   (UDIBreakType) type,
			   (BreakIdType *) break_id)) <= TIPFAILURE) {
     PrintErrorMessage (UDIretval);
     return (FAILURE);
  } else if (UDIretval == SUCCESS) {
     return(SUCCESS);
  } else {
     udi_warning(UDIretval);
     return(FAILURE);
  }
}

/* Query   (get status)  Breakpoints   */

INT32
/**********************************************Mini_bkpt_stat   */
Mini_bkpt_stat(break_id, address, m_space, pass_count, 
	       bkpt_type, current_cnt)
  int       break_id;
  INT32       *m_space;
  ADDR32      *address;
  INT32       *pass_count;
  INT32       *bkpt_type;
  INT32       *current_cnt;
{
  UDIError	UDIretval;
  UDIResource	addr;

  if ((UDIretval = UDIQueryBreakpoint ((BreakIdType) break_id,
				&addr,
				(UDIInt32 *)pass_count,
				(UDIBreakType *) bkpt_type,
				(UDIInt32 *) current_cnt)) <= TIPFAILURE) {
    PrintErrorMessage (UDIretval);
    return (FAILURE);
  } else if (UDIretval == SUCCESS) {
    *address = addr.Offset;
    *m_space = xlate_mspace_udi2mon(addr.Space);
    if (*bkpt_type & MONBreakFlagHardware)
	*bkpt_type = BKPT_29050;
    return(SUCCESS);
  } else {
     if (UDIretval == UDIErrorNoMoreBreakIds)
	return ((INT32) MONBreakNoMore);
     else if (UDIretval == UDIErrorInvalidBreakId)
	return ((INT32) MONBreakInvalid);
     else {
       udi_warning(UDIretval);
       return(FAILURE);
     }
  };
}

/* Kill    Target     */

INT32
/**********************************************Mini_break   */
Mini_break()
{

  UDIStop();
  return (SUCCESS);
}

/* Obtain Target configuration and resynchronize with target  */

UDIInt32
/**********************************************Mini_config_req   */
Mini_config_req(target_config, versions)
  TARGET_CONFIG  *target_config;
  VERSIONS_ETC	 *versions;
{
  UDIError	UDIretval;
  UDIMemoryRange	DFEMemRange[MONMaxMemRanges];
  UDIUInt32	ChipVersions[MONMaxChips];
  UDIInt	NumRanges, NumChips;
  UDIInt	i;

  NumRanges = (UDIInt) MONMaxMemRanges;
  NumChips  = (UDIInt) MONMaxChips;

  if ((UDIretval = UDIGetTargetConfig(
			   (UDIMemoryRange *) &DFEMemRange[0],
			   (UDIInt *) &NumRanges, /* 3 -> I, D, R */
			   (UDIUInt32 *) &ChipVersions[0],
			   (UDIInt *) &NumChips)) <= TIPFAILURE) {
      PrintErrorMessage (UDIretval);
      return(FAILURE);
  } else if ((UDIretval == SUCCESS) || (UDIretval == UDIErrorIncomplete)) {
      if (UDIretval == UDIErrorIncomplete) {
        fprintf(stderr, "Ignoring: ");
        if (io_config.echo_mode == (INT32) TRUE) {
           fprintf(io_config.echo_file, "Ignoring: ");
	   fflush (io_config.echo_file);
	}
	udi_warning(UDIretval);
      };
      i = (UDIInt) 0;
      while ((i < (UDIInt) MONMaxMemRanges) && (i < NumRanges)) {
	   switch ((int) DFEMemRange[i].Space) {
	     case  UDI29KDRAMSpace:
		target_config->D_mem_start = (ADDR32) DFEMemRange[i].Offset;
		target_config->D_mem_size = (INT32) DFEMemRange[i].Size;
		break;
	     case	 UDI29KIROMSpace:
		target_config->ROM_start = (ADDR32) DFEMemRange[i].Offset;
		target_config->ROM_size = (INT32) DFEMemRange[i].Size;
		break;
	     case	 UDI29KIRAMSpace:
		target_config->I_mem_start = (ADDR32) DFEMemRange[i].Offset;
		target_config->I_mem_size = (INT32) DFEMemRange[i].Size;
		break;
	     default: /* don't care, so ignore it */
		break;
	   };
	   i = i + (UDIInt) 1;
      } /* end while */
      i = (UDIInt) 0;
      while ((i < (UDIInt) MONMaxChips) && (i < NumChips)) {
	   switch (i) {
	      case	0:  /* cpu */
		target_config->processor_id = (UINT32) ChipVersions[i];
		break;
	      case	1:  /* coprocessor */
		target_config->coprocessor = (UINT32) ChipVersions[i];
		if (target_config->coprocessor == (UINT32) UDI29KChipNotPresent)
		   target_config->coprocessor = (UINT32) -1; /* MONDFE's */
		break;
	      default:  /* ignore */
		break;
	   };
	   i = i + (UDIInt) 1;
      } /* end while */
      return(SUCCESS);
  } else {
      udi_warning(UDIretval);
      return(FAILURE);
  }
}


/* Copy memory and registers    */


INT32
/**********************************************Mini_copy   */
Mini_copy(src_space, src_addr, dst_space, dst_addr, byte_count, size, dir)
  INT32        src_space,
               dst_space;
  ADDR32       src_addr,
               dst_addr;
  INT32      	 byte_count;
  INT16		size;
  INT32		 dir;
{
  UDIError	UDIretval;
  UDIResource	from, to;
  UDICount	count_done;

  from.Space = xlate_mspace_mon2udi(src_space);
  from.Offset = src_addr;
  to.Space = xlate_mspace_mon2udi(dst_space);
  to.Offset = dst_addr;

  if ((UDIretval = UDICopy (from,
			    to,
			    (UDICount) byte_count,
			    (UDISizeT) size,
			    &count_done,
			    (UDIBool) dir)) <= TIPFAILURE) {
      PrintErrorMessage (UDIretval);
      return (FAILURE);
  } else if (UDIretval == SUCCESS) {
      return(SUCCESS);
  } else {
     udi_warning(UDIretval);
     return(FAILURE);
  }
}

/* Fill memory and registers    */


/* fill_count if greater than 4 should be a multiple of 4 */
INT32
/**********************************************Mini_fill   */
Mini_fill(m_space, start, fill_count, pattern_count, pattern)
  INT32        m_space;
  ADDR32          start;
  INT32        fill_count,
                  pattern_count;
  BYTE           *pattern;
{
  UDIBool         host_endian;
  UDIResource	from;
  UDICount	count_done;
  UDIError	UDIretval;
  
  host_endian = FALSE;

  from.Offset = start;
  from.Space = xlate_mspace_mon2udi (m_space);

  if (fill_count == (INT32) 1) { /* takes only one write */
     if ((UDIretval = UDIWrite((UDIHostMemPtr) pattern,
			       from,
			       (UDICount) fill_count,
			       (UDISizeT) pattern_count, /* a byte at a time */
			       (UDICount *) &count_done,
			       host_endian)) <= TIPFAILURE) {
        PrintErrorMessage (UDIretval);
        return (FAILURE);
     } else if (UDIretval == SUCCESS) {
	return(SUCCESS);
     } else {
        udi_warning(UDIretval);
        return(FAILURE);
     };
  } else {
     /* Handle arbitrary length strings to Data memory separately */
     if ((pattern_count > (INT32) 4) && 
			       ((int) (pattern_count % 4) != (int) 0)){
	if (from.Space != UDI29KDRAMSpace)
	  return (FAILURE);
	return((INT32) FillString(from, (UDIHostMemPtr) pattern,
			   (UDISizeT) pattern_count, (UDICount) fill_count));
     } else {
	return((INT32) FillWords(from, (UDIHostMemPtr) pattern, 
			   (UDISizeT) pattern_count, (UDICount) fill_count));
     }
  };
}

/* Initiate a wait and get target status  */

INT32
/**********************************************Mini_get_target_stats   */
Mini_get_target_stats(maxtime, target_status)
INT32	maxtime;
INT32  *target_status;
{
  UDIPId	pid;
  UDIError	UDIretval;
  UDIInt32	udiwait_code;

  if (maxtime == (INT32) -1) {
    udiwait_code = (UDIInt32) UDIWaitForever;
  } else {
    udiwait_code = (UDIInt32) maxtime;
  };
  if ((UDIretval = UDIWait ((UDIInt32) udiwait_code,
			    &pid,
			    (UDIUInt32 *) target_status)) <= TIPFAILURE) {
        PrintErrorMessage (UDIretval);
        return (SUCCESS);  /* considered non-fatal */
  } else if (UDIretval == SUCCESS) {
	CurrentPID = (UDIPId) pid; /* useful when reconnecting */
	return(SUCCESS);
  } else {
        udi_warning(UDIretval);
        return(FAILURE);
  };
}


INT32
/**********************************************Mini_go   */
Mini_go()
{
  UDIError	UDIretval;

  if ((UDIretval = UDIExecute()) <= TIPFAILURE) {
        PrintErrorMessage (UDIretval);
        return (FAILURE);
  } else if (UDIretval == SUCCESS) {
	return(SUCCESS);
  } else {
        udi_warning(UDIretval);
        return(FAILURE);
  };
}

INT32
/**********************************************Mini_init   */
Mini_init(txt_start, txt_end, dat_start, dat_end, 
	  entry_point, m_stack, r_stack,
	  arg_string)
  ADDR32          txt_start,
                  txt_end,
                  dat_start,
                  dat_end;
  ADDR32          entry_point;
  INT32        m_stack,
                  r_stack;
  char		*arg_string;
{
  UDIMemoryRange	ProcessMemory[MONMaxProcessMemRanges];
  UDIInt	NumRanges;
  UDIResource	Entry;
  CPUSizeT	StackSizes[MONMaxStacks];
  UDIInt	NumStacks;
  UDIError	UDIretval;

  NumRanges = (UDIInt) MONMaxProcessMemRanges;
  NumStacks = (UDIInt) MONMaxStacks;
  ProcessMemory[0].Space = (CPUSpace) UDI29KIRAMSpace;
  ProcessMemory[0].Offset = (CPUOffset) txt_start;
  ProcessMemory[0].Size = (CPUSizeT) (txt_end - txt_start);
  ProcessMemory[1].Space = (CPUSpace) UDI29KDRAMSpace;
  ProcessMemory[1].Offset = (CPUOffset) dat_start;
  ProcessMemory[1].Size = (CPUSizeT) (dat_end - dat_start);
  Entry.Offset = entry_point;
  Entry.Space = xlate_mspace_mon2udi((INT32) I_MEM);
  StackSizes[0] = (CPUSizeT) r_stack;
  StackSizes[1] = (CPUSizeT) m_stack;

  if ((UDIretval = UDIInitializeProcess (&ProcessMemory[0],
					 (UDIInt) NumRanges,
					 Entry,
					 &StackSizes[0],
					 (UDIInt) NumStacks,
					 arg_string)) <= TIPFAILURE) {
        PrintErrorMessage (UDIretval);
	return (FAILURE);
  } else if (UDIretval == SUCCESS) {
	return(SUCCESS);
  } else {
        udi_warning(UDIretval);
        return(FAILURE);
  };
}


/* Read memory or registers from target  */
INT32
/**********************************************Mini_read_req   */
Mini_read_req(m_space, address, byte_count, size, count_done,
	      buffer, host_endian)
  INT32        m_space;
  ADDR32       address;
  INT32        byte_count;
  INT16	       size;
  INT32        host_endian;
  INT32       *count_done;
  BYTE        *buffer;
{
  UDIError	UDIretval;
  UDIResource	from;

  from.Space = xlate_mspace_mon2udi(m_space);
  from.Offset = address;

  if ((UDIretval = UDIRead (from,
			    (UDIHostMemPtr) buffer,
			    (UDICount) byte_count,
			    (UDISizeT) size,
			    (UDICount *) count_done,
			    (UDIBool) host_endian)) <= TIPFAILURE) {
       PrintErrorMessage (UDIretval);
      return(FAILURE);
  } else if (UDIretval == SUCCESS) {
      return(SUCCESS);
  } else {
      udi_warning(UDIretval);
      return(FAILURE);
  }
}

/* 
 * Reset target processor   
 */
INT32
/**********************************************Mini_reset_processor   */
Mini_reset_processor()
{
  UDIMemoryRange	ProcessMemory[MONMaxProcessMemRanges];
  UDIInt	NumRanges;
  UDIResource	Entry;
  CPUSizeT	StackSizes[MONMaxStacks];
  UDIInt	NumStacks;
  UDIError	UDIretval;
  UDIPId	CurrentPID;
  UDIUInt32	StopReason;


  NumRanges = (UDIInt) MONMaxProcessMemRanges;
  NumStacks = (UDIInt) MONMaxStacks;
  ProcessMemory[0].Space = (CPUSpace) UDI29KIRAMSpace;
  ProcessMemory[0].Offset = (CPUOffset) 0;
  ProcessMemory[0].Size = (CPUSizeT) 0;
  ProcessMemory[0].Space = (CPUSpace) UDI29KIRAMSpace;
  ProcessMemory[0].Offset = (CPUOffset) 0;
  ProcessMemory[0].Size = (CPUSizeT) 0;
  Entry.Offset = 0;
  Entry.Space = xlate_mspace_mon2udi((INT32) I_MEM);
  StackSizes[0] = (CPUSizeT) 0;
  StackSizes[1] = (CPUSizeT) 0;

  if ((UDIretval = UDIWait((UDIInt32) 0, &CurrentPID, &StopReason))
							 <= TIPFAILURE) {
       PrintErrorMessage (UDIretval);
      return(FAILURE);
  } else if (UDIretval != SUCCESS) {
      udi_warning(UDIretval);
      return(FAILURE);
  };
  /* set PID to ProcProcessor */
  if (( UDIretval = UDISetCurrentProcess((UDIPId) UDIProcessProcessor))
							   <= TIPFAILURE) {
       PrintErrorMessage (UDIretval);
      return(FAILURE);
  } else if (UDIretval != SUCCESS) {
      udi_warning(UDIretval);
      return(FAILURE);
  };
  /* Successful */
  /* call InitializeProcess. Paramters ignored. */
  if ((UDIretval = UDIInitializeProcess (&ProcessMemory[0],
					 (UDIInt) NumRanges,
					 Entry,
					 &StackSizes[0],
					 (UDIInt) NumStacks,
					 (char *) 0)) <= TIPFAILURE) {
       PrintErrorMessage (UDIretval);
      return(FAILURE);
  } else if (UDIretval != SUCCESS) {
      udi_warning(UDIretval);
      return(FAILURE);
  };
  /* Successful */
  if (( UDIretval = UDISetCurrentProcess((UDIPId) CurrentPID))
							 <= TIPFAILURE) {
       PrintErrorMessage (UDIretval);
      return(FAILURE);
  } else if (UDIretval != SUCCESS) {
      udi_warning(UDIretval);
      return(FAILURE);
  };
  return (SUCCESS);
}

/* Write memory or registers to target  */
INT32
/**********************************************Mini_write_req   */
Mini_write_req(m_space, address, byte_count, size, count_done,
	       buffer, host_endian)
  INT32        m_space;
  ADDR32          address;
  INT32        byte_count;
  INT16		size;
  INT32       *count_done;
  BYTE           *buffer;
  INT32        host_endian;
{
  UDIError	UDIretval;
  UDIResource	to;

  to.Space = xlate_mspace_mon2udi(m_space);
  to.Offset = address;

  if ((UDIretval = UDIWrite((UDIHostMemPtr) buffer,
			    to,
			    (UDICount) byte_count,
			    (UDISizeT) size,
			    (UDICount *) count_done,
			    (UDIBool) host_endian)) <= TIPFAILURE) {
        PrintErrorMessage (UDIretval);
       return(FAILURE);
  } else if (UDIretval == SUCCESS) {
      return(SUCCESS);
  } else {
      udi_warning(UDIretval);
      return(FAILURE);
  }
}

INT32
Mini_stdin_mode_x(mode)
INT32	*mode;
{
  UDIError	UDIretval;

  if ((UDIretval = UDIStdinMode ((UDIMode *) mode)) <= TIPFAILURE) {
        PrintErrorMessage (UDIretval);
        return (FAILURE);
  } else if (UDIretval == SUCCESS) {
	return(SUCCESS);
  } else {
        udi_warning(UDIretval);
        return(FAILURE);
  };
}

INT32
/**********************************************Mini_put_stdin   */
Mini_put_stdin(buffer, bufsize, count)
  char       *buffer;
  INT32        bufsize;
  INT32       *count;
{
  UDIError	UDIretval;

  if ((UDIretval = UDIPutStdin ((UDIHostMemPtr) buffer,
				(UDISizeT) bufsize,
				(UDISizeT *) count)) <= TIPFAILURE) {
        PrintErrorMessage (UDIretval);
        return (FAILURE);
  } else if (UDIretval == SUCCESS) {
	return(SUCCESS);
  } else {
        udi_warning(UDIretval);
        return(FAILURE);
  };
}

/* Put characters to stdout   */


INT32
/**********************************************Mini_get_stdout   */
Mini_get_stdout(buffer, bufsize, count_done)
  char       *buffer;
  INT32       *count_done;
  INT32        bufsize;
{
  UDIError	UDIretval;

  if ((UDIretval = UDIGetStdout ((UDIHostMemPtr) buffer,
			 (UDISizeT) bufsize,
			 (UDISizeT *) count_done)) <= TIPFAILURE) {
        PrintErrorMessage (UDIretval);
        return (FAILURE);
  } else if (UDIretval == SUCCESS) {
	return(SUCCESS);
  } else {
        udi_warning(UDIretval);
        return(FAILURE);
  }
}

INT32
/**********************************************Mini_get_stderr   */
Mini_get_stderr(buffer, bufsize, count_done)
  char       *buffer;
  INT32       *count_done;
  INT32        bufsize;
{
  UDIError	UDIretval;
  if ((UDIretval = UDIGetStderr ((UDIHostMemPtr) buffer,
			 (UDISizeT) bufsize,
			 (UDISizeT *) count_done)) <= TIPFAILURE) {
        PrintErrorMessage (UDIretval);
        return (FAILURE);
  } else if (UDIretval == SUCCESS) {
	return(SUCCESS);
  } else {
        udi_warning(UDIretval);
        return(FAILURE);
  }
}

/* Step instructions   */


INT32
/**********************************************Mini_step   */
Mini_step(count)
INT32	count;
{
  UDIError	UDIretval;
  UDIRange	dummy_range;
  UDIStepType	step_type;

  dummy_range.Low = 0;
  dummy_range.High = 0xffffffff;

  step_type = UDIStepNatural;

  if ((UDIretval = UDIStep((UDIUInt32) count,
			   step_type,
			   dummy_range)) <= TIPFAILURE) {
        PrintErrorMessage (UDIretval);
        return (FAILURE);
  } else if (UDIretval == SUCCESS) {
	return(SUCCESS);
  } else {
        udi_warning(UDIretval);
        return(FAILURE);
  };
}

INT32
/***************************************************** Mini_put_trans */
Mini_put_trans(buffer)
char	*buffer;
{
  UDIError	UDIretval;
  UDISizeT	count;
  UDISizeT	*count_done;

  count = (UDISizeT) (strlen (buffer) + 1);
  if ((UDIretval = UDIPutTrans((UDIHostMemPtr) buffer,
			       (UDISizeT) count,
			       (UDISizeT *) count_done)) <= TIPFAILURE) {
        PrintErrorMessage (UDIretval);
        return (FAILURE);
  } else if (UDIretval == SUCCESS) {
	return(SUCCESS);
  } else {
        udi_warning(UDIretval);
        return(FAILURE);
  };
  
}


static CPUSpace
xlate_mspace_mon2udi(m_space)
INT32	m_space;
{
  switch(m_space) {
    case	LOCAL_REG:
	return((CPUSpace) UDI29KLocalRegs);
    case	ABSOLUTE_REG:
	return((CPUSpace) UDI29KGlobalRegs);
    case	GLOBAL_REG:
	return((CPUSpace) UDI29KGlobalRegs);
    case	SPECIAL_REG:
	return((CPUSpace) UDI29KSpecialRegs);
    case	TLB_REG:
	return((CPUSpace) UDI29KTLBRegs);
    case	COPROC_REG:
	return((CPUSpace) UDI29KAm29027Regs);
    case	I_MEM:
	return((CPUSpace) UDI29KIRAMSpace);
    case	D_MEM:
	return((CPUSpace) UDI29KDRAMSpace);
    case	I_ROM:
	return((CPUSpace) UDI29KIROMSpace);
    case	D_ROM:
	return((CPUSpace) UDI29KDRAMSpace);
    case	I_O:
	return((CPUSpace) UDI29KIOSpace);
    case	I_CACHE:
	return((CPUSpace) UDI29KICacheSpace);
    case	D_CACHE:
	return((CPUSpace) UDI29KDCacheSpace);
    case	PC_SPACE:
	return((CPUSpace) UDI29KPC);
    case	A_SPCL_REG:
	return((CPUSpace) UDI29KSpecialRegs);
    case	GENERIC_SPACE:
	return ((CPUSpace) UDI29KDRAMSpace);
    case	VERSION_SPACE:
	return ((CPUSpace) VERSION_SPACE);
    default:
	return((CPUSpace) FAILURE);
  };
}

static INT32
xlate_mspace_udi2mon(mspace)
CPUSpace  	mspace;
{
   switch(mspace) {
     case	UDI29KDRAMSpace:
	return((INT32) D_MEM);
     case	UDI29KIOSpace:
	return((INT32) I_O);
     case	UDI29KCPSpace0:
	return((INT32) FAILURE);
     case	UDI29KCPSpace1:
	return((INT32) FAILURE);
     case	UDI29KIROMSpace:
	return((INT32) I_ROM);
     case	UDI29KIRAMSpace:
	return((INT32) I_MEM);
     case	UDI29KLocalRegs:
	return((INT32) LOCAL_REG);
     case	UDI29KGlobalRegs:
	return((INT32) GLOBAL_REG);
     case	UDI29KRealRegs:
	return((INT32) GLOBAL_REG);
     case	UDI29KSpecialRegs:
	return((INT32) SPECIAL_REG);
     case	UDI29KTLBRegs:
	return((INT32) TLB_REG);
     case	UDI29KACCRegs:
	return((INT32) FAILURE);
     case	UDI29KICacheSpace:
	return((INT32) I_CACHE);
     case	UDI29KAm29027Regs:
	return((INT32) COPROC_REG);
     case	UDI29KPC:
	return((INT32) PC_SPACE);
     case	UDI29KDCacheSpace:
	return((INT32) D_CACHE);
     default:
	return(FAILURE);
   };
}

static
void udi_warning(num)
int	num;
{
  fprintf(stderr, "UDIERROR: %d : %s\n", num, udi_errmsg[num]);
  fflush(stderr);
  if (io_config.echo_mode == (INT32) TRUE) {
     fprintf(io_config.echo_file, "UDIERROR: %d :%s\n", num, udi_errmsg[num]);
     fflush (io_config.echo_file);
  }
}

static void
PrintErrorMessage(UDIretval)
UDIError	UDIretval;
{
    UDISizeT	ErrorMsgCnt;

    fprintf(stderr, "TIPERROR: %d :", UDIretval);
    fflush(stderr);
    if (io_config.echo_mode == (INT32) TRUE) {
       fprintf(io_config.echo_file, "TIPERROR: %d :", UDIretval);
       fflush(io_config.echo_file);
    }

    ErrorMsgCnt = (UDISizeT) 0;
    do {
       if (UDIGetErrorMsg(UDIretval, 
			     (UDISizeT) MONErrorMsgSize, 
			     MONErrorMsg, &ErrorMsgCnt) != UDINoError) {
	   fprintf(stderr, "TIPERROR: Could not get TIP error message.\n");
	   fflush(stderr);
	   return;
	}
       write (fileno(stderr), &MONErrorMsg[0], (int) ErrorMsgCnt);
       if (io_config.echo_mode == (INT32) TRUE)
          write (fileno(io_config.echo_file), &MONErrorMsg[0], (int) ErrorMsgCnt);
    } while (ErrorMsgCnt == (UDISizeT) MONErrorMsgSize);
    fprintf(stderr, "\n");
    if (io_config.echo_mode == (INT32) TRUE) {
       fprintf(io_config.echo_file, "\n");
       fflush(io_config.echo_file);
    }
    return;
}

static UDIError
FillWords(from, pattern, pattern_count, fill_count)
UDIResource	from;
UDIHostMemPtr	pattern;
UDISizeT	pattern_count;
UDICount	fill_count;
{
   UDICount	count_done;
   UDIBool	host_endian, direction;
   UDIError	UDIretval;
   UDIResource	to;

   INT32	isregspace = ISREG(xlate_mspace_udi2mon(from.Space));

   host_endian = FALSE;

        if ((UDIretval = UDIWrite((UDIHostMemPtr) pattern,
			       from,
			       (UDICount) 1,
			       (UDISizeT) pattern_count,
			       (UDICount *) &count_done,
			       host_endian)) <= TIPFAILURE) {
           PrintErrorMessage (UDIretval);
           return (FAILURE);
        } else if (UDIretval == SUCCESS) {  /* do copy */
	   fill_count = fill_count - 1; /* one less */
	   if (fill_count > (INT32) 0) { /* do copy */
	      if (isregspace)
	   	   to.Offset = from.Offset + (pattern_count/4);
	      else		
		   to.Offset = from.Offset + pattern_count;
	      to.Space = from.Space; /* already translated */
	      direction = TRUE; /* front to back */
	      if (pattern_count > (INT32) 4) { /* is a multiple of 4 also */
	         fill_count = (INT32) (fill_count * (pattern_count/4));
	         pattern_count = (INT32) 4;
	      };
	      if ((UDIretval = UDICopy (from,
			     to,
			     fill_count,
			     (UDISizeT) pattern_count,
			     (UDICount *) &count_done,
			     direction)) <= TIPFAILURE) {
                  PrintErrorMessage (UDIretval);
                  return (FAILURE);
	      } else if (UDIretval == SUCCESS) {
	         return(SUCCESS);
	      } else {
                 udi_warning(UDIretval);
                 return(FAILURE);
	      }
	   };
	   /* return if no more to copy */
	   return(SUCCESS);
        } else {
           udi_warning(UDIretval);
           return(FAILURE);
        };
}

static UDIError
FillString(from, pattern, pattern_count, fill_count)
UDIResource	from;
UDIHostMemPtr	pattern;
UDISizeT	pattern_count;
UDICount	fill_count;
{
   UDICount	count_done;
   UDIBool	host_endian, direction;
   UDIError	UDIretval;
   UDIResource	to;

   host_endian = FALSE;

        if ((UDIretval = UDIWrite((UDIHostMemPtr) pattern,
			       from,
			       (UDICount) pattern_count,
			       (UDISizeT) 1,
			       (UDICount *) &count_done,
			       host_endian)) <= TIPFAILURE) {
           PrintErrorMessage (UDIretval);
           return (FAILURE);
        } else if (UDIretval == SUCCESS) {  /* do copy */
	   fill_count = fill_count - 1; /* one less */
	   if (fill_count > (INT32) 0) { /* do copy */
	      to.Offset = from.Offset + pattern_count;
	      to.Space = from.Space; 
	      direction = TRUE; /* front to back */
	      if ((UDIretval = UDICopy (from,
			     to,
			     (UDICount) (fill_count*pattern_count),
			     (UDISizeT) 1,
			     (UDICount *) &count_done,
			     direction)) <= TIPFAILURE) {
                  PrintErrorMessage (UDIretval);
                  return (FAILURE);
	      } else if (UDIretval == SUCCESS) {
	         return(SUCCESS);
	      } else {
                 udi_warning(UDIretval);
                 return(FAILURE);
	      }
	   };
	   /* return if no more to copy */
	   return(SUCCESS);
        } else {
           udi_warning(UDIretval);
           return(FAILURE);
        };
}
