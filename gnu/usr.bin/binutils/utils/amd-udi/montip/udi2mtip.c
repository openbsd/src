static char     _[] = "@(#)udi2mtip.c	5.31 93/11/03 08:34:07, Srini, AMD.";
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
 *      Engineers: Srini Subramanian.
 *****************************************************************************
 * This module implements the UDI procedural interface routines of MONTIP
 * for both the Dos and Unix environments.
 *****************************************************************************
 */
#include <stdio.h>
#include  <signal.h>

#ifdef	MSDOS
#include <stdlib.h>
#include <conio.h>
#else
#include <malloc.h>
#endif

#include <string.h>
#include "coff.h"
#include "messages.h"
#include "memspcs.h"
#include "macros.h"
#include "udiproc.h"
#include "udiids.h"
#include "udiext.h"
#include "mtip.h"
#include "hif.h"
#include "versions.h"

/* 
 * MsgCode halt1, halt2, halt3, halt4 are variables defined as INT32 *
 * inside the macro block *
 */


#define	CLEAR_PENDING_STOP	StopFlag=0;

/* Stop signal handler / macro */
#define	STOP_SIG_HDLR	{\
			INT32   MsgCode;\
			INT32   halt1, halt2, halt3, halt4;\
			StopFlag=0;\
			Mini_build_break_msg();\
			if (Mini_msg_send() != SUCCESS)\
			  return((-1) * MONErrCantSendMsg);\
			SIGINT_POLL	\
			MsgCode = Wait_For_Ack();\
			if (MsgCode == ABORT_FAILURE)\
			   return ((-1) * MONErrAbortAborted);\
			if (MsgCode == FAILURE)\
			   return ((-1) * MONErrNoAck);\
			else if (MsgCode != HALT)\
			   return ((-1) * MONErrCantRecvMsg);\
			Mini_unpack_halt_msg(&halt1, &halt2, &halt3, &halt4);\
			};

#define	SEND_AND_WAIT_ACK(x)	{\
				INT32	MsgCode;\
				if (Mini_msg_send() != SUCCESS)\
				   return((-1)*MONErrCantSendMsg);\
				SIGINT_POLL	\
				MsgCode = Wait_For_Ack();\
				if (MsgCode == ABORT_FAILURE)\
				  return (UDIErrorAborted);\
				else if (MsgCode == FAILURE)\
				  return ((-1) * MONErrNoAck);\
				else if (MsgCode == ERROR)\
				  ReturnedError = 1; \
				else if (MsgCode != (INT32) (x))\
				  return ((-1) * MONErrCantRecvMsg);\
				};

#define	MONUDISession		1

static int      AllSections=(STYP_ABS|STYP_TEXT|STYP_LIT|STYP_DATA|STYP_BSS);
static UDIPId   CurrentPID = (UDIPId) UDIProcessProcessor;
static UDIUInt32 PreviousProcessorState;
static UDIUInt32 ProcessorState;
static int      TipAlive = 0;
static	int	NumberOfProcesses=0;
static int      ContinuingSession = 0;
static char    *TargetType;
static char    *SecondTarget;
static char    *CoreFile;
static int      CoreLoaded;
static BreakIdType LastBreakId = 0;
static UDIBool  SupervisorMode;
static UDIBool  RealMode;
static UDIBool  ProtectedMode;
static UDIBool  VirtualMode;
static int      BreaksInPlace;	/* EB29K */
static int      StepCmdGiven = 0;
static int	ReturnedError=0;
static	int	StopFlag=0;
static	int	Interrupted=0;
static	int	RemoteTarget=0;
static	int	NoStepReqd=0;
static	int	NoChan1Ack=0;
static	int	SendACKFirst=0;
static	INT32	MsgAlreadyInBuffer = 0;
static	INT32	MsgAlreadyReceived = 0;
static	int	Channel0Busy=0;
unsigned long	TimeOut;
int		MessageRetries;
int		BlockCount;
int		DelayFactor;
unsigned int	MaxMsgBufSize;
extern int		lpt_initialize;	/* global */
extern int		use_parport;		/* global */
static	UDISizeT	ErrCntRemaining=(UDISizeT) 0;
static	FILE	*coff_in;
static	char	buffer[LOAD_BUFFER_SIZE];

/* used in input/output routines */
#define	TIP_IO_BUFSIZE		1024
static 	char     	channel0_buffer[TIP_IO_BUFSIZE];
static 	UDISizeT   	Channel0_count=0;
static 	char     	channel1_buffer[TIP_IO_BUFSIZE];
static 	UDISizeT   	Channel1_count=0;
static	char		channel2_buffer[TIP_IO_BUFSIZE];
static 	UDISizeT   	Channel2_count=0;
static 	UDIUInt32 	Lr4_count;
static 	UDIUInt32 	TotalDone=(UDIUInt32) 0;
static 	CPUOffset 	Lr3_addr;

#define	TIP_COOKED	0		/* default */
#define	TIP_RAW		1
#define	TIP_CBREAK	2
#define	TIP_ECHO	4
#define	TIP_ASYNC	8
#define	TIP_NBLOCK	0x10
static	UDIUInt32	PgmStdinMode=TIP_COOKED;	/* default */
static	UDIUInt32	StdinCharsNeeded=0;

/* Cache register values */
static UDIUInt32 Glob_Regs[128],
                Loc_Regs[128];
static int      RefreshRegs = 1;
static int	  exitstat;
static	char	ConnectString[512];
static	char		TempArgString[1024];

static	struct tip_break_table  *bp_table=NULL;

/* Global variables */
TIP_TARGET_CONFIG tip_target_config;
TIP_TARGET_STATUS tip_target_status;
TIP_CONFIG      tip_config;
char           *Msg_Logfile;
FILE           *MsgFile;

/* ------------- Minimon TIP Specific Error Codes ------------ */
#define	MONNoError		0
#define	MONErrCantSendMsg	1
#define	MONErrCantRecvMsg	2
#define	MONErrCantLoadROMfile	3
#define	MONErrCantInitMsgSystem	4
#define	MONErrCantBreakInROM	5
#define	MONErrCantResetComm	6
#define	MONErrCantAllocBufs	7
#define	MONErrUnknownBreakType	8
#define	MONErrNoAck		9
#define	MONErrNoSynch		10
#define	MONErrCantOpenCoff	11
#define	MONErrCantWriteToMem	12
#define	MONErrAbortAborted	13
#define	MONErrNullConfigString	14
#define	MONErrNoTargetType	15
#define	MONErrOutofMemory	16
#define	MONErrErrorInit		17
#define	MONErrErrorRead		18
#define	MONErrErrorWrite	19
#define	MONErrErrorCopy		20
#define	MONErrErrorSetBreak	21
#define	MONErrErrorStatBreak	22
#define	MONErrErrorRmBreak	23
#define	MONErrConfigInterrupt	24
#define	MONErrNoConfig		25
#define	MONErrMsgInBuf		26
#define	MONErrUnknownTIPCmd	27
#define	MAX_MONERR		28

static char    *monerr_tip[] = {
   /* 0 */ "No Error.",
   /* 1 */ "Could not send message to target.",
   /* 2 */ "Did not receive the correct ACK from target.",
   /* 3 */ "Cant load ROM file.",
   /* 4 */ "Cant initialize the message system.",
   /* 5 */ "Cant set breakpoint in ROM.",
   /* 6 */ "Cant reset communication channel.",
   /* 7 */ "Cant reallocate message buffers.",
   /* 8 */ "Breakpoint type requested is not recognized.",
   /* 9 */ "No ACK from target - timed out.",
   /* 10 */ "Timed out synching. No response from target.",
   /* 11 */ "Cannot open ROM file.",
   /* 12 */ "Cannot write to memory while downloading ROM file.",
   /* 13 */ "Ctrl-C aborted previous Ctrl-C processing.",
   /* 14 */ "Null configuration string specified for connection.",
   /* 15 */ "No Target type specified for connection.",
   /* 16 */ "Out of memory.",
   /* 17 */ "Error on target - trying to initialize process.",
   /* 18 */ "Error on target - trying to read.",
   /* 19 */ "Error on target - trying to write.",
   /* 20 */ "Error on target - trying to do copy.",
   /* 21 */ "Error on target - trying to set breakpoint.",
   /* 22 */ "Error on target - trying to query breakpoint.",
   /* 23 */ "Error on target - trying to remove breakpoint.",
   /* 24 */ "User interrupt signal received - Aborting synch.",
   /* 25 */ "Couldn't get target config after reset. Try again.",
   /* 26 */ "Message received from target waiting in buffer.",
   /* 27 */ "Unknown Montip command, Exiting TIP mode."
};

#define	MAX_MONERR_SIZE		80

/* ---------------- Error Codes -------------------------------- */

/* Function declarations */

extern	void	IntHandler PARAMS((int num));
extern	void	print_recv_bytes PARAMS((void));
extern	void	set_lpt PARAMS((void));
extern	void	unset_lpt PARAMS((void));
static	FILE	*FindFile PARAMS((char *));
static	char	*GetTargetType PARAMS((char *, char *));
static	INT32	SendConfigWait PARAMS((void));
extern	void	SendACK PARAMS((void));
static 	int 	parse_string PARAMS((char *string));
static 	int 	write_args PARAMS((char *argstr, ADDR32 argstart,
						     ADDR32 * datahigh));
static 	int 	write_argv PARAMS((int arg_count, char *arg_ptr[], 
				   ADDR32 argstart, ADDR32 * hi_data));
static	INT32 	SpaceMap_udi2mm PARAMS((CPUSpace space));
static	CPUSpace	SpaceMap_mm2udi PARAMS((INT32 space));
static	int 	Reset_Processor PARAMS((void));
static	INT32 	Wait_For_Ack PARAMS((void));
static	void 	process_target_msg PARAMS((INT32 msgcode));
static	void 	process_HALT_msg PARAMS((void));
static	INT32 	process_chan0_ack PARAMS((void));
static	void 	process_CHAN1_msg PARAMS((void));
static	void 	process_CHAN2_msg PARAMS((void));
static	void 	process_stdin_needed_req PARAMS((void));
static	void 	set_stdin_mode PARAMS((void));
static	void 	process_ERR_msg PARAMS((void));
static	void 	process_HIF_msg PARAMS((void));
static 	int 	PutAllBreakpoints PARAMS((void));
static 	int 	ResetAllBreakpoints PARAMS((void));
static 	int 	Write_Glob_Reg PARAMS((INT32 RegVal, int RegNum));

/*
 * these three functions are called from HIF/IO handlers to do terminal
 * input/output.
 */
extern	void 	set_stdin_needed PARAMS((ADDR32 offset, UDICount count));
extern	void 	set_stderr_ready PARAMS((ADDR32 offset, UDICount count));
extern	void 	set_stdout_ready PARAMS((ADDR32 offset, UDICount count));


static	INT32  	Mini_load_coff PARAMS((char *fname,
			       INT32 space, 
			       INT32 sym,
			       INT32 sects,
			       int   msg));
static	int 	update_breakpt_at PARAMS((INT32 space, ADDR32 addr, ADDR32 Inst));
static	int 	is_breakpt_at PARAMS((INT32 space, ADDR32 addr));
static	void 	add_to_bp_table PARAMS((BreakIdType * id, INT32 space,
		ADDR32 offset, INT32 count, INT32 type, ADDR32 inst));
static	int 	get_from_bp_table PARAMS((BreakIdType id, INT32 * space,
				ADDR32 * offset, INT32 * count,
				INT32 * type, ADDR32 * inst));
static	int 	remove_from_bp_table PARAMS((BreakIdType id));

extern	INT32 	CheckForMsg PARAMS((INT32 time));

extern	int 	service_HIF PARAMS((UINT32 svcnm, UINT32 lr2, UINT32 lr3, 
		   UINT32 lr4, UINT32 * gr96, UINT32 * gr97, UINT32 * gr121));

/* ================================================================= */
/* UDI Procedure definitions  */

UDIError 
UDIConnect(string, Session)
  char           *string;
  UDISessionId   *Session;
{
  INT32           MsgCode;
  int		 retval;

  if (TipAlive) {	/* already connected */
    /* If same TargetType requested, return ConnectionUnavailable */
    SecondTarget = NULL;
    if ((SecondTarget = GetTargetType (SecondTarget, string)) == NULL)
       return (UDIErrorInvalidTIPOption);
    if (strcmp (SecondTarget, TargetType) == 0)
       return (UDIErrorConnectionUnavailable);
    else
       return (UDIErrorTryAnotherTIP);
  } else {
    if (ContinuingSession) {
      ContinuingSession=0; /* reset */
      *Session = (UDISessionId) MONUDISession;
      if ((int) (ProcessorState & 0xFF) != UDINotExecuting) {/* none active */
        CurrentPID = (UDIPId) (UDIProcessProcessor+1);
        NumberOfProcesses=1;
      };
      TipAlive = 1;
      return (UDINoError);
    }
    /* Initialize variables */
    /* Take control of Ctrl-C until connect time */
    signal (SIGINT, IntHandler);
    CoreFile = NULL;
    TargetType = NULL;
    Msg_Logfile = NULL;
    CoreLoaded = 0;
    CurrentPID = (UDIPId) UDIProcessProcessor;
    SupervisorMode = 0;
    RealMode = 0;
    ProtectedMode = 1;	/* default */
    VirtualMode = 0;
    BreaksInPlace = 0;	/* EB29K */
    TimeOut = (unsigned long) 10000;
    MessageRetries = (int) 1000;
#ifdef	MSDOS
    BlockCount = (int) 1000;
#else
    BlockCount = (int) 40000;
#endif
    DelayFactor = (int) 0;
    MaxMsgBufSize = (unsigned int) 0;
    Channel0_count = 0;
    Channel1_count = 0;
    Channel2_count = 0;
    Channel0Busy=0;
    *Session = (UDISessionId) MONUDISession;
    TipAlive = 1; /* no more positive error codes */

    /* TIP_CONFIG initialization */
    tip_config.PC_port_base = (INT32) - 1;	/* default */
    tip_config.PC_mem_seg = (INT32) - 1;	/* default */
    (void) strcpy(tip_config.baud_rate, DEFAULT_BAUD_RATE);
    (void) strcpy(tip_config.comm_port, DEFAULT_COMM_PORT);
    (void) strcpy(tip_config.par_port, DEFAULT_PAR_PORT);
    /* Get the CFG register value to find out 29kEndian */
    tip_target_config.P29KEndian = BIG;	/* default */

    if ((retval = parse_string(string)) != (int) 0)
      return ((UDIError) retval);	
    if (TargetType == NULL)
       return ((-1) * MONErrNoTargetType);

    /* Open Msg_Logfile if any */
    if (Msg_Logfile) {
      if ((MsgFile = fopen(Msg_Logfile, "w")) == NULL)
	Msg_Logfile = NULL;
    }
    /* Initialize message system */
    if (Mini_msg_init(TargetType) != SUCCESS) {
     *Session = (UDISessionId) MONUDISession;
      TipAlive = 1;
      return ((-1) * MONErrCantInitMsgSystem);
    }
    /* Reset communication channel */
    Mini_reset_comm();

    /*
     * Should we have different TIPS: one for shared memory & another for
     * serial connections?
     */
    if (CoreFile) {
      if ((MsgCode = Mini_load_coff(CoreFile, (INT32) D_MEM, (INT32) 1, (INT32) AllSections, 0)) != SUCCESS) {
       *Session = (UDISessionId) MONUDISession;
	TipAlive = 1;
	return ((UDIError) MsgCode);
      }
      CoreLoaded = 1;	/* True */
      /* Reset communication channel */
      Mini_reset_comm();
      Mini_go_target();	/* RESET Target Processor */
    }

    /* Define TIPs endianess */

#ifdef	MSDOS
    tip_target_config.TipEndian = LITTLE;
#else
    tip_target_config.TipEndian = BIG;
#endif

   if (strcmp(TargetType, "serial") &&
       strcmp(TargetType, "paral_1") &&
       strcmp(TargetType, "pcserver")) { /* non-serial targets */
     RemoteTarget = 0;	/* not a remote target */
   } else {
     RemoteTarget = 1;	/* remote target */
     SendACKFirst = 1; /* safe to send always */
   }

   if (RemoteTarget == 0) {	/* shared memory */
      MsgCode = Wait_For_Ack();
      if (MsgCode == ABORT_FAILURE) 
	 return (UDIErrorAborted);
      else if (MsgCode == FAILURE) 
	return((-1) * MONErrNoSynch);
    }

    if (SendACKFirst)
	 SendACK();
    /* send a config msg to sync with target */
    do {
       MsgCode = SendConfigWait();
    } while (MsgCode == HALT);
    if (MsgCode == ABORT_FAILURE)
       return (UDIErrorAborted);
    else if (MsgCode == FAILURE)
	return ((-1) * MONErrNoSynch);
    else if (MsgCode != CONFIG)
	return ((-1) * MONErrCantRecvMsg);
    Mini_unpack_config_msg(&tip_target_config);

    /* Reallocate message buffers to the smallest, if necessary */
    if ((MaxMsgBufSize != (unsigned int) 0) && 
	    (MaxMsgBufSize < (unsigned int) tip_target_config.max_msg_size))
       tip_target_config.max_msg_size = (INT32) MaxMsgBufSize;

    if (Mini_alloc_msgbuf((int) tip_target_config.max_msg_size) != SUCCESS)
      return ((-1) * MONErrCantAllocBufs);

     ProcessorState = (UDIUInt32) UDINotExecuting;
     PreviousProcessorState = (UDIUInt32) UDINotExecuting;

    return (UDINoError);
  }
}

UDIError 
UDIDisconnect(Session, Terminate)
  UDISessionId    Session;
  UDIBool	Terminate;
{
  if (Session != (UDISessionId) MONUDISession)
    return (UDIErrorNoSuchConnection);

  if (Terminate == (UDIBool) UDITerminateSession) {
    if (CoreFile)
      (void) free((char *) CoreFile);
    if (TargetType)
      (void) free((char *) TargetType);
    Mini_msg_exit();	/* clean up message buffers */
    if (Msg_Logfile)
      (void) fclose(MsgFile);
    ProcessorState = (UDIUInt32) UDINotExecuting;
    PreviousProcessorState = (UDIUInt32) UDINotExecuting;
  } else {
    ContinuingSession=1;
  };
  if ((int) (ProcessorState & 0xFF) == UDINotExecuting) {/* none active */
     CurrentPID = (UDIPId) UDIProcessProcessor;
     NumberOfProcesses=0;
  }
  TipAlive = 0;
  return (UDINoError);
}

UDIError 
UDISetCurrentConnection(Session)
  UDISessionId    Session;
{
  if (Session != (UDISessionId) MONUDISession)
    return (UDIErrorNoSuchConnection);

  return (UDINoError);
}

UDIError 
UDICapabilities ( TIPId, TargetId, DFEId, DFE, TIP, DFEIPCId, TIPIPCId, TIPString) 
  UDIUInt32	*TIPId;			/* Out */
  UDIUInt32	*TargetId;		/* Out */
  UDIUInt32	DFEId;			/* In */
  UDIUInt32	DFE;			/* In */
  UDIUInt32	*TIP;			/* Out */
  UDIUInt32	*DFEIPCId;		/* Out */
  UDIUInt32	*TIPIPCId;		/* Out */
  char		*TIPString;		/* Out */
{
  *TIPId = (UDIUInt32) UDIID (UDIProductCode_Montip, MONTIPRev, MONTIPSubRev, MONTIPSubSubRev);
  *TargetId = (UDIUInt32) UDIID (UDIProductCode_Montip, MONTIPRev, MONTIPSubRev, MONTIPSubSubRev);
  if ((int) (DFE & 0x00000FFF) > (int) (MONTIPUDIVers)) {
     *TIP = (UDIUInt32) 0;
  } else if ((int) (DFE & 0x00000FFF) == (int) MONTIPUDIVers) {
     *TIP = (UDIUInt32) DFE;
  } else {
     *TIP = (UDIUInt32) MONTIPUDIVers;
  }
  *DFEIPCId = (UDIUInt32) 0;
  *TIPIPCId = (UDIUInt32) 0;
  (void) strcpy (TIPString, "UDI 1.2 Conformant Montip for 29K targets\0");
  return (UDINoError);
}

UDIError 
UDIGetErrorMsg (ErrorCode, MsgSize, Msg, CountDone)
  UDIError	ErrorCode;		/* In */
  UDISizeT	MsgSize;		/* In */
  char		*Msg;			/* Out */
  UDISizeT	*CountDone;		/* Out */
{
  int		index;

  /* Continue Previous Error Message */
  if (ErrCntRemaining != (UDISizeT) 0) {
    index = (int) (strlen(monerr_tip[-ErrorCode]) + 1 - ErrCntRemaining);
    if (MsgSize < (UDISizeT) ErrCntRemaining) {
      (void) strncpy((char *) Msg, (char *) (monerr_tip[-ErrorCode]+index), MsgSize);
      *CountDone = MsgSize;
      ErrCntRemaining = ErrCntRemaining - MsgSize;
    } else {
      (void) strcpy((char *) Msg, (char *) (monerr_tip[-ErrorCode]+index));
      *CountDone = (UDISizeT) strlen(Msg) + 1;
      ErrCntRemaining = (UDISizeT) 0;
    }
    return (UDINoError);
  };
  /* A New ErrorCode */
  if ((ErrorCode <= 0) && (ErrorCode > (-1) * MAX_MONERR)) {
    if (MsgSize < (UDISizeT) MAX_MONERR_SIZE) {
      (void) strncpy((char *) Msg, monerr_tip[-ErrorCode], MsgSize);
      *CountDone = MsgSize;
      ErrCntRemaining = (UDISizeT) (strlen(monerr_tip[-ErrorCode])+1) - MsgSize;
    } else {
      (void) strcpy((char *) Msg, monerr_tip[-ErrorCode]);
      *CountDone = (UDISizeT) strlen(Msg) + 1;
      ErrCntRemaining = (UDISizeT) 0;
    }
    return (UDINoError);
  } else {
    return (UDIErrorUnknownError);
  };
}

UDIError 
UDIGetTargetConfig(KnownMemory, NumberOfRanges, ChipVersions, NumberOfChips) 
  UDIMemoryRange KnownMemory[];		/* Out */
  UDIInt	*NumberOfRanges;	/* In/Out */
  UDIUInt32	ChipVersions[];		/* Out */
  UDIInt	*NumberOfChips;		/* In/Out */
{
  UDIInt	InRanges, InChips;
  int		Incomplete;

  Incomplete = 0;

  InRanges = *NumberOfRanges;
  InChips = *NumberOfChips;

  if ((InRanges < (UDIInt) MONMaxMemRanges) || (InChips < (UDIInt) MONMaxChips))
     Incomplete = 1;
   
  *NumberOfRanges = (UDIInt) 0;
  switch ((int) InRanges) {
    default:
    case	3 /* MONMaxMemRanges */:
     if (*NumberOfRanges == (UDIInt) 0)
	 *NumberOfRanges = (UDIInt) 3;
     KnownMemory[2].Space = (CPUSpace) UDI29KIRAMSpace;
     KnownMemory[2].Offset = (CPUOffset) tip_target_config.I_mem_start;
     KnownMemory[2].Size = (CPUSizeT) tip_target_config.I_mem_size;
    case	2:
     if (*NumberOfRanges == (UDIInt) 0)
	 *NumberOfRanges = (UDIInt) 2;
     KnownMemory[1].Space = (CPUSpace) UDI29KDRAMSpace;
     KnownMemory[1].Offset = (CPUOffset) tip_target_config.D_mem_start;
     KnownMemory[1].Size = (CPUSizeT) tip_target_config.D_mem_size;
    case	1:
     if (*NumberOfRanges == (UDIInt) 0)
	 *NumberOfRanges = (UDIInt) 1;
     KnownMemory[0].Space = (CPUSpace) UDI29KIROMSpace;
     KnownMemory[0].Offset = (CPUOffset) tip_target_config.ROM_start;
     KnownMemory[0].Size = (CPUSizeT) tip_target_config.ROM_size;
	break;
    case	0:
        *NumberOfRanges = (UDIInt) 0;
	break;
  }

  *NumberOfChips = (UDIInt) 0;
  switch ((int) InChips) {
     default:
     case	2: /* MONMaxChips */
       if (*NumberOfChips == (UDIInt) 0) 
	   *NumberOfChips = (UDIInt) 2;
       if (tip_target_config.coprocessor == (UINT32) -1)
          ChipVersions[1] = (UDIUInt32) UDI29KChipNotPresent;
       else
          ChipVersions[1] = (UDIUInt32) tip_target_config.coprocessor;
     case	1:
       if (*NumberOfChips == (UDIInt) 0) 
	   *NumberOfChips = (UDIInt) 1;
        ChipVersions[0] = (UDIUInt32) tip_target_config.processor_id;
	break;
     case	0:
	*NumberOfChips = (UDIInt) 0;
	break;
  }
  if (Incomplete)
     return (UDIErrorIncomplete);
  else
     return (UDINoError);
}

UDIError 
UDICreateProcess(pid)
  UDIPId         *pid;
{
  if (CurrentPID == (UDIPId) (UDIProcessProcessor + 1))
    return (UDIErrorCantCreateProcess);
  CurrentPID = (UDIPId) (UDIProcessProcessor + 1);
  NumberOfProcesses=1;
  *pid = (UDIPId) CurrentPID;
  return (UDINoError);
}

UDIError 
UDISetCurrentProcess(pid)
  UDIPId          pid;
{
  if ((pid > (UDIPId) (UDIProcessProcessor + 1)) ||
	 (pid < (UDIPId) (UDIProcessProcessor)))
    return (UDIErrorNoSuchProcess);
  if ((NumberOfProcesses == (int) 0) && (pid != (UDIPId) UDIProcessProcessor))
    return (UDIErrorNoSuchProcess);
  CurrentPID = pid;
  return (UDINoError);
}

UDIError 
UDIDestroyProcess(pid)
  UDIPId          pid;
{
  if ((pid > (UDIPId) (UDIProcessProcessor + 1)) ||
	 (pid < (UDIPId) (UDIProcessProcessor)))
    return (UDIErrorNoSuchProcess);
  CurrentPID = (UDIPId) UDIProcessProcessor;
  ProcessorState = (UDIUInt32) UDINotExecuting;
  PreviousProcessorState = (UDIUInt32) UDINotExecuting;
  NumberOfProcesses=0;
  return (UDINoError);
}

UDIError 
UDIInitializeProcess (ProcessMemory, NumberOfRanges, EntryPoint, StackSizes, NumberOfStacks, ArgString)
  UDIMemoryRange ProcessMemory[];	/* In */
  UDIInt	NumberOfRanges;		/* In */
  UDIResource	EntryPoint;		/* In */
  CPUSizeT	StackSizes[];		/* In */
  UDIInt	NumberOfStacks;		/* In */
  char		*ArgString;		/* In */
{
  UDIError        ErrCode;
  UDIRange	  text_addr, data_addr;
  CPUSizeT	  mem_stack_size, reg_stack_size;

  ADDR32          arg_start;
  ADDR32          data_high;
  ADDR32          highmem;
  INT32           os_control;
  INT32		MsgCode;
  UDIInt	i;


  exitstat = 0;	/* reset */

  PgmStdinMode=TIP_COOKED;	/* revert to default mode */

  CLEAR_PENDING_STOP

  if (CurrentPID == (UDIPId) UDIProcessProcessor) {
    if ((MsgCode = Reset_Processor()) != SUCCESS)
       return ((UDIError) MsgCode);
    do {
       MsgCode = SendConfigWait();
    } while (MsgCode == HALT);
    if (MsgCode == ABORT_FAILURE)
       return (UDIErrorAborted);
    else if (MsgCode == FAILURE)
	return ((-1) * MONErrNoSynch);
    else if (MsgCode != CONFIG)
	return ((-1) * MONErrCantRecvMsg);
    Mini_unpack_config_msg(&tip_target_config);
    /* Reallocate message buffers */
    if (Mini_alloc_msgbuf((int) tip_target_config.max_msg_size) != SUCCESS)
      return ((-1) * MONErrCantAllocBufs);
    ProcessorState = (UDIUInt32) UDINotExecuting;
    Channel0_count = 0;
    Channel1_count = 0;
    Channel2_count = 0;
    Channel0Busy = 0;
    PreviousProcessorState = (UDIUInt32) UDINotExecuting;
    return (UDINoError);
  };


  /* For other processes */
  /* Set Default Values */
  mem_stack_size = (CPUSizeT) MONDefaultMemStackSize;
  reg_stack_size = (CPUSizeT) MONDefaultRegStackSize;
  text_addr.Low = (CPUOffset) tip_target_config.I_mem_start;
  text_addr.High = (CPUOffset) tip_target_config.I_mem_start +
		   (CPUOffset) tip_target_config.I_mem_size - 1;
  data_addr.Low = (CPUOffset) tip_target_config.D_mem_start;
  data_addr.High = (CPUOffset) tip_target_config.D_mem_start +
		   (CPUOffset) tip_target_config.D_mem_size -
		   (CPUOffset) (mem_stack_size + reg_stack_size + 16) - 1;

  /* Get Memory Ranges */
  if (NumberOfRanges != (UDIInt) 0) {
     for (;NumberOfRanges--;) {
       switch ((int) ProcessMemory[NumberOfRanges].Space) {
	  case	UDI29KIRAMSpace:
		text_addr.Low = ProcessMemory[NumberOfRanges].Offset;
		text_addr.High = ProcessMemory[NumberOfRanges].Offset +
			(CPUOffset) ProcessMemory[NumberOfRanges].Size;
		break;
	  case	UDI29KDRAMSpace:
		data_addr.Low = ProcessMemory[NumberOfRanges].Offset;
		data_addr.High = ProcessMemory[NumberOfRanges].Offset +
			(CPUOffset) ProcessMemory[NumberOfRanges].Size;
		break;
	  default: /* don't care */
		break;
       } /* switch */
     } /* for */
  }
  /* Get Stack Sizes */
  for (i = (UDIInt) 0; i < NumberOfStacks; i=i+(UDIInt)1) {
     switch ((int) i) {
	case	0:  /* register stack size */
		if (StackSizes[0] != (CPUSizeT) 0)
		  reg_stack_size = StackSizes[0];
		break;
	case	1: /* memory stack size */
		if (StackSizes[1] != (CPUSizeT) 0)
		  mem_stack_size = StackSizes[1];
		break;
	default: /* don't care */
		break;
     }
  }

  if ((CPUOffset) text_addr.High > (CPUOffset) data_addr.High)
     data_addr.High = text_addr.High;  /* when no data sections */
  arg_start = (data_addr.High + 7) & ~0x7;	/* word boundary */

  if ((ErrCode = write_args(ArgString, 
			    arg_start, &data_high)) != UDINoError)
    return (ErrCode);

  data_addr.High = (data_high + 7) & ~0x7;	/* double word bdry */

  highmem = (ADDR32) 0;

  /* User programs run mode */
  if (SupervisorMode)
    os_control = (INT32) 0x10000000;	/* set bit 28 only */
  else if (VirtualMode || ProtectedMode)
    os_control = (INT32) 0;
  else
    os_control = (INT32) 0x80000000;

  Mini_build_init_msg((ADDR32) text_addr.Low, (ADDR32) text_addr.High,
		      (ADDR32) data_addr.Low, (ADDR32) data_addr.High,
		      (ADDR32) EntryPoint.Offset,
		      (INT32) mem_stack_size, (INT32) reg_stack_size,
		      (ADDR32) highmem,
		      (ADDR32) arg_start,
		      (INT32) os_control);
  SEND_AND_WAIT_ACK(INIT_ACK);
  if (ReturnedError == (int) 1) {
    ReturnedError = 0;
    return ((-1) * MONErrErrorInit);
  }
  Mini_unpack_init_ack_msg();

  ProcessorState = (UDIUInt32) UDINotExecuting;
  PreviousProcessorState = (UDIUInt32) UDINotExecuting;

  return (UDINoError);
}

UDIError 
UDIRead(from, to, count, size, count_done, host_endian)
  UDIResource     from;
  UDIHostMemPtr   to;
  UDICount        count;
  UDISizeT          size;
  UDICount       *count_done;
  UDIBool         host_endian;
{
  INT32           space = SpaceMap_udi2mm(from.Space);
  INT32           done;
  INT32           ttl_count;
  INT32           msg_count;
  INT32           overhead;

  ADDR32          ack_addr;
  INT32		  ack_space;
  BYTE           *output;
  UDIError        UDIretval;

  UDICount        i;
  INT32          *Version;

  int             Gr1_val;
  int             Lrnum;
  int             j;
  UDIResource     temp_from;
  UDICount        temp_done;
  UDIUInt32       start_offset,
                  end_offset;
  BYTE           *reg_data;


  CLEAR_PENDING_STOP

  if (count <= (UDICount) 0) {
    *count_done = (UDICount) 0;
    return (UDINoError);
  }

  if (space == (INT32) VERSION_SPACE) {	/* minimon ver cmd */
    Version = (INT32 *) to;
    *Version = (INT32) tip_target_config.version;
     *(Version+1) = (INT32) tip_target_config.os_version; 
    /*  TIPVERSION must be 11 chars or less  */
    strcpy((char *) (Version+2),TIPVERSION); 
    /*  TIPDATE must be 11 chars or less  */
    strcpy((char *) (Version+5),TIPDATE); 
    /* max msg size */
    *(Version + 8) = tip_target_config.max_msg_size;
    /* max bkpts */
    *(Version + 9) = tip_target_config.max_bkpts;
    if ((host_endian) && (tip_target_config.TipEndian != tip_target_config.P29KEndian)) {
      output = (BYTE *) to;
      for (i = 0; i < count; i++) {
        if (size == 4)
	  tip_convert32(output);
        else if (size == 2)
	  tip_convert16(output);
        output = output + size;
      }
    }	/* hostendian */
    *count_done = (UDICount) count;
    return (UDINoError);
  };

  if (space < (INT32) 0) {
    *count_done = (UDICount) 0;
    return (UDIErrorUnknownResourceSpace);
  }

  output = (BYTE *) to;

  switch (from.Space) {
   case UDI29KPC:
    from.Offset = 1;	/* PC1 */
    break;
   case UDI29KGlobalRegs:
    break;
   case UDI29KRealRegs:
    /* REAL REGS BEGIN */
    /* get global and local reg values from target if target exec'ed */
    if (RefreshRegs) {
      RefreshRegs = 0;	/* reset */
      temp_from.Offset = (CPUOffset) 0;
      temp_from.Space = UDI29KGlobalRegs;
      if ((UDIretval = UDIRead(temp_from,
			       (UDIHostMemPtr) &Glob_Regs[0],
			       (UDICount) 2,
			       (UDISizeT) 4,
			       (UDICount *) &temp_done,
			       (UDIBool) TRUE)) != UDINoError)	/* gr0, gr1 */
	return (UDIretval);
      /* UDIRead ();  gr64 to gr 127 */
      temp_from.Offset = (CPUOffset) 64;
      temp_from.Space = UDI29KGlobalRegs;
      if ((UDIretval = UDIRead(temp_from,
			       (UDIHostMemPtr) &Glob_Regs[64],
			       (UDICount) 64,
			       (UDISizeT) 4,
			       (UDICount *) &temp_done,
			       (UDIBool) TRUE)) != UDINoError)	/* gr0, gr1 */
	return (UDIretval);
      /* UDIRead ();   lr0 to lr127 */
      temp_from.Offset = (CPUOffset) 0;
      temp_from.Space = UDI29KLocalRegs;
      if ((UDIretval = UDIRead(temp_from,
			       (UDIHostMemPtr) &Loc_Regs[0],
			       (UDICount) 128,
			       (UDISizeT) 4,
			       (UDICount *) &temp_done,
			       (UDIBool) TRUE)) != UDINoError)	/* gr0, gr1 */
	return (UDIretval);
    };

    start_offset = from.Offset;
    end_offset = start_offset + count;
    output = (BYTE *) to;
    while (start_offset < end_offset) {	/* do only if count is non zero */
      if (start_offset <= (UDIUInt32) 127) {
	reg_data = (BYTE *) &Glob_Regs[(int) start_offset];
	for (j = 0; j < 4 /* sizeof (UDIUInt32) */ ; j++)
	  *output++ = *reg_data++;
      } else if ((start_offset >= (UDIUInt32) 128) && (start_offset <= (UDIUInt32) 255)) {
	Gr1_val = (int) (Glob_Regs[1] & 0x000001FC) >> 2;	/* bits 2 to 8 */
	Lrnum = (int) ((int) start_offset - Gr1_val) % 128;
	reg_data = (BYTE *) & Loc_Regs[(int) Lrnum];
	for (j = 0; j < 4 /* sizeof (UDIUInt32) */ ; j++)
	  *output++ = *reg_data++;
      } else
	return (UDIErrorUnknownResourceSpace);
      start_offset = start_offset + (UDIUInt32) 1;
    }	/* end while */
    *count_done = (UDICount) count;
    return (UDINoError);
    /* REAL REGS END */
   default:
    break;
  }

  output = (BYTE *) to;
  if ( (RemoteTarget == 0) &&
	((from.Space == UDI29KDRAMSpace) ||
	 (from.Space == UDI29KIRAMSpace) ||
	 (from.Space == UDI29KIROMSpace))) { /* shared memory board */
      Mini_read_memory(space, from.Offset, count * size, (BYTE *) output);
  } else {
     /* overhead = checksum + header + size rounding + bfr rounding + ? */
     overhead = 32;
     ttl_count = count;
     output = (BYTE *) to;
     while (ttl_count > 0) {	
       /* Check for user interrupt */
       if (StopFlag) {
          STOP_SIG_HDLR
          ProcessorState = (UDIUInt32) UDIStopped;
          PreviousProcessorState = (UDIUInt32) UDIStopped;
          return (UDIErrorAborted);
       }; 
       /* Check possible buffer overflow */
       if ((ttl_count * size) + overhead > 
#ifdef MSDOS
		   tip_target_config.max_msg_size) {
         msg_count = (tip_target_config.max_msg_size-overhead) >> (size >> 1);  
#else
		   (INT32) 256) { /* SunOS has problems with higher numbers */
         msg_count = (256 - overhead) >> (size >> 1);  
#endif
         ttl_count = ttl_count - msg_count;
       } else {
         msg_count = ttl_count;
         ttl_count = ttl_count - msg_count;
       }
       Mini_build_read_req_msg(space, (ADDR32) from.Offset, msg_count, size);
       SEND_AND_WAIT_ACK(READ_ACK);
       if (ReturnedError == (int) 1) {
	 ReturnedError = 0;
	 return ((-1) * MONErrErrorRead);
       }
       Mini_unpack_read_ack_msg((INT32 *) &ack_space, (ADDR32 *) &ack_addr,
			     (INT32 *) &done, (BYTE *) output);
       output = output + (msg_count * size);
       if (ISMEM(space))
          from.Offset = from.Offset + (CPUOffset) (msg_count * size);
       else
          from.Offset = from.Offset + (CPUOffset) msg_count;
     }
  } /* end while */

  if ((host_endian) && 
	(tip_target_config.TipEndian != tip_target_config.P29KEndian)) {
    output = (BYTE *) to;
    for (i = 0; i < count; i++) {
      if (size == 4)
	tip_convert32(output);
      else if (size == 2)
	tip_convert16(output);
      output = output + size;
    }
  }	/* hostendian */

  *count_done = (UDICount) count;
  return (UDINoError);
}

UDIError 
UDIWrite(from, to, count, size, count_done, HostEndian)
  UDIHostMemPtr   from;
  UDIResource     to;
  UDICount        count;
  UDISizeT          size;
  UDICount       *count_done;
  UDIBool         HostEndian;
{
  INT32           space = SpaceMap_udi2mm(to.Space);
  INT32           done;
  INT32           ttl_count;
  INT32           msg_count;
  INT32           overhead;
  ADDR32          ack_addr;
  INT32		  ack_space;
  BYTE           *input;
  UDIError        UDIretval;
  UDIUInt32       tmpbuf[2];
  UDICount		i;

  /* REAL REGS BEGIN */
  UDIResource     temp_to;
  UDICount        temp_done;
  CPUOffset       start_offset,
                  end_offset;
  UDIUInt32       Gr1_val;

  /* REAL REGS END */

  CLEAR_PENDING_STOP

  if (space < (INT32) 0) {
    *count_done = (UDICount) 0;
    return (UDIErrorUnknownResourceSpace);
  }

  if (count <= (UDICount) 0) {
    *count_done = (UDICount) 0;
    return (UDINoError);
  }

  if (to.Space == UDI29KPC) {
    /* when writing UDI29KPC, set both PC1 and PC0 */
    /* NOTE: this assumes we are not in freeze mode */
    /* this must all be done before doing the endian conversion below */
    to.Offset = 0;	/* start at PC0 */
    count = (UDIInt32) 2;	/* writing 2 4-byte quantities */
    tmpbuf[1] = *((UDIUInt32 *) from);	/* PC1 = PC */
    if (!HostEndian && (tip_target_config.TipEndian != tip_target_config.P29KEndian)) {
	tmpbuf[0] = tmpbuf[1];
	tip_convert32((BYTE *) &tmpbuf[0]);
        tmpbuf[0] = tmpbuf[0] + 4;	/* PC0 = PC + 4 */
	tip_convert32((BYTE *) &tmpbuf[0]);
    } else {
        tmpbuf[0] = tmpbuf[1] + 4;	/* PC0 = PC + 4 */
    }
    from = (UDIHostMemPtr) tmpbuf;	/* set pointer to temporary (8-byte)
					 * buffer */
  }

  switch (to.Space) {
   case UDI29KLocalRegs:
    RefreshRegs = 1;
    break;
   case UDI29KPC:	/* PC causes special regs(PC0,PC1) space */
    break;
   case UDI29KGlobalRegs:
    RefreshRegs = 1;
    break;
   case UDI29KRealRegs:
    RefreshRegs = 1;
    /* REAL REGS BEGIN */
    start_offset = to.Offset;
    end_offset = start_offset + count - 1;
    if ((end_offset <= 127)) {	/* all globals asked */
      temp_to.Offset = to.Offset;
      temp_to.Space = UDI29KGlobalRegs;
      if ((UDIretval = UDIWrite(from,
				temp_to,
				count,
				size,
				&temp_done,
				HostEndian)) != UDINoError)
	return (UDIretval);
    } else if (start_offset > 127) {	/* all local regs */
      /* read gr1 */
      temp_to.Offset = (CPUOffset) 1;
      temp_to.Space = UDI29KGlobalRegs;
      if ((UDIretval = UDIRead(temp_to,
			       (UDIHostMemPtr) &Gr1_val,
			       (UDICount) 1,
			       (UDISizeT) 4,
			       (UDICount *) &temp_done,
			       (UDIBool) TRUE)) != UDINoError)	/* gr1 */
	return (UDIretval);
      /* recompute start_offset and end_offset */
      Gr1_val = (Gr1_val & 0x01FC) >> 2;
      start_offset = (start_offset - Gr1_val) % 128;
      end_offset = (end_offset - Gr1_val) % 128;
      input = (BYTE *) from;
      if (start_offset > end_offset) {	/* wrap around */
	temp_to.Offset = start_offset;
	temp_to.Space = UDI29KLocalRegs;
	if ((UDIretval = UDIWrite(input,
				  temp_to,
				  (UDICount) (128 - start_offset),
				  size,
				  &temp_done,
				  HostEndian)) != UDINoError)
	  return (UDIretval);
	input = input + (int) ((128 - start_offset) * size);
	temp_to.Offset = (CPUOffset) 0;	/* from LR0 */
	temp_to.Space = UDI29KLocalRegs;
	if ((UDIretval = UDIWrite(input,
				  temp_to,
				  (UDICount) (end_offset + 1 ),
				  size,
				  &temp_done,
				  HostEndian)) != UDINoError)
	  return (UDIretval);
      } else {	/* no wrapping */
	temp_to.Offset = start_offset;
	temp_to.Space = UDI29KLocalRegs;
	if ((UDIretval = UDIWrite(input,
				  temp_to,
				  count,
				  size,
				  &temp_done,
				  HostEndian)) != UDINoError)
	  return (UDIretval);
      }
    } else {	/* overlap */
      input = (BYTE *) from;
      /* write globals */
      temp_to.Offset = start_offset;
      temp_to.Space = UDI29KGlobalRegs;
      if ((UDIretval = UDIWrite(input,
				temp_to,
				((UDICount) 128 - (UDICount) start_offset),
				size,
				&temp_done,
				HostEndian)) != UDINoError)
	return (UDIretval);
      input = input + (int) (size) * ((UDICount) 128 - (UDICount) start_offset);
      /* write locals */
      temp_to.Offset = (CPUOffset) 128;
      temp_to.Space = UDI29KRealRegs;
      if ((UDIretval = UDIWrite(input,
				temp_to,
				(UDICount) (count - 128 + start_offset),
				size,
				&temp_done,
				HostEndian)) != UDINoError)
	return (UDIretval);
    }
    *count_done = (UDICount) count;
    return (UDINoError);
    /* REAL REGS END */
   default:
    break;
  }

  if (HostEndian && 
	      (tip_target_config.TipEndian != tip_target_config.P29KEndian)) {
    input = (BYTE *) from;
    for (i = 0; i < count; i++) {
      if (size == 4)
	tip_convert32(input);
      else if (size == 2)
	tip_convert16(input);
      input = input + size;
    }
  }; /* endian conversion done */

  input = (BYTE *) from;
  if ((RemoteTarget == 0) &&
	((to.Space == UDI29KDRAMSpace) ||
	 (to.Space == UDI29KIRAMSpace) ||
	 (to.Space == UDI29KIROMSpace))) {
      Mini_write_memory(space, to.Offset, count * size, (BYTE *) input);
      *count_done = (UDICount) count;
      return (UDINoError);
  } else {  /* remote */
     /* overhead = checksum + header + size rounding + bfr rounding + ? */
     overhead = 32;
     ttl_count = count;
     input = (BYTE *) from;
     while (ttl_count > 0) {	
       /* Check for user interrupt */
       if (StopFlag) {
          STOP_SIG_HDLR
          ProcessorState = (UDIUInt32) UDIStopped;
          PreviousProcessorState = (UDIUInt32) UDIStopped;
          return (UDIErrorAborted);
       }; 
       /* Check possible buffer overflow */
       if ((ttl_count * size) + overhead > 
		   tip_target_config.max_msg_size) {
         msg_count = (tip_target_config.max_msg_size-overhead) >> (size >> 1);  
         ttl_count = ttl_count - msg_count;
       } else {
         msg_count = ttl_count;
         ttl_count = ttl_count - msg_count;
       }
       Mini_build_write_req_msg(space, (ADDR32) to.Offset,
			      msg_count, size, (BYTE *) input);
       SEND_AND_WAIT_ACK(WRITE_ACK);
       if (ReturnedError == (int) 1) {
	 ReturnedError = 0;
	 return ((-1) * MONErrErrorWrite);
       }
       Mini_unpack_write_ack_msg((INT32 *) &ack_space,
			      (ADDR32 *) &ack_addr,
			      (INT32 *) &done);
       input = input + (msg_count * size);
       if (ISMEM(space))
          to.Offset = to.Offset + (CPUOffset) (msg_count * size);
       else
          to.Offset = to.Offset + (CPUOffset) msg_count;
     }	/* while */
  } /* end remote */
  *count_done = (to.Space == UDI29KPC) ? (UDICount) 1 : (UDICount) count;
  return (UDINoError);
}

UDIError 
UDICopy(from, to, count, size, count_done, direction)
  UDIResource     from;
  UDIResource     to;
  UDICount        count;
  UDISizeT          size;
  UDICount       *count_done;
  UDIBool         direction;
{
  INT32           f_space = SpaceMap_udi2mm(from.Space);
  INT32           t_space = SpaceMap_udi2mm(to.Space);

  UDICount	  counter, maxcount,curcount;
  INT32	  	fill_count, fill_size;
  
  INT32           done;
  ADDR32          ack_saddr,
                  ack_daddr;

  CLEAR_PENDING_STOP

  if ((t_space < 0) || (f_space < 0)) {
    *count_done = (UDICount) 0;
    return (UDIErrorUnknownResourceSpace);
  }

  if (count <= (UDICount) 0) {
    *count_done = (UDICount) 0;
    return (UDINoError);
  }

  RefreshRegs = 1;

  /* Split the copy to smaller copies based on the message size */
  maxcount = (UDICount) (tip_target_config.max_msg_size / size);
  counter = (UDICount) count;

  while (counter > (UDICount) 0) {
    /* Check for user interrupt */
    if (StopFlag) {
       STOP_SIG_HDLR
       ProcessorState = (UDIUInt32) UDIStopped;
       PreviousProcessorState = (UDIUInt32) UDIStopped;
       return (UDIErrorAborted);
    }; 
    curcount = (maxcount < counter) ? maxcount : counter;
    counter = counter - curcount;
    if ((size > (UDISizeT) 4) && (t_space == (INT32) I_MEM)) { 
       /* reduce it to  4, must be a multiple also for I_MEM */
       fill_count = (INT32) (curcount * (size/4));
       fill_size = (INT32) size;
    } else if ((size > (UDISizeT) 4) && (t_space != (INT32) I_MEM)) { 
       /* copy as bytes */
       fill_count = (INT32) (curcount * size);
       fill_size = (INT32) 1; /* bytes */
    } else {
       fill_count = (INT32) curcount;
       fill_size = (INT32) size;
    };
    Mini_build_copy_msg(f_space, (ADDR32) from.Offset,
		      t_space, (ADDR32) to.Offset,
		      fill_count, fill_size);
    SEND_AND_WAIT_ACK(COPY_ACK);
    if (ReturnedError == (int) 1) {
      ReturnedError = 0;
      return ((-1) * MONErrErrorCopy);
    }
    Mini_unpack_copy_ack_msg(&f_space, &ack_saddr,
			   &t_space, &ack_daddr, &done);
    from.Offset = from.Offset + (CPUOffset) (curcount * size);
    to.Offset = to.Offset + (CPUOffset) (curcount * size);
  }; /* end while */

  *count_done = (UDICount) count;
  return (UDINoError);
}

UDIError 
UDIExecute()
{
  INT32           MsgCode;

    CLEAR_PENDING_STOP

  if (!NoStepReqd) {
    if (!StepCmdGiven) {
      /* Execute one instruction */
      Mini_build_step_msg((INT32) 1);

      if (Mini_msg_send() != SUCCESS)
	return ((-1) * MONErrCantSendMsg);
      /* process message received from target */
      MsgCode = Wait_For_Ack();
      if (MsgCode == ABORT_FAILURE)
	return (UDIErrorAborted);
      else if (MsgCode == FAILURE)
	return ((-1) * MONErrNoAck);

      process_target_msg(MsgCode);

      /* if processor state is stepped, set breakpoints, issue a GO */
      if (ProcessorState != (UDIUInt32) UDIStepped) {
	RefreshRegs = 1;
	return (UDINoError);
      }
      PutAllBreakpoints();
      BreaksInPlace = 1;
    }
  }

  Mini_build_go_msg();

  if (Mini_msg_send() != SUCCESS)
    return ((-1) * MONErrCantSendMsg);

  RefreshRegs = 1;
  ProcessorState = (UDIUInt32) UDIRunning;
  PreviousProcessorState = (UDIUInt32) UDIRunning;

  return (UDINoError);
}

/*
 * Stepping will NOT cause any breakpoints to be installed. It will step the
 * number requested.
 */

UDIError 
UDIStep(steps, steptype, range)
  UDIUInt32       steps;
  UDIStepType     steptype;
  UDIRange        range;
{
  CLEAR_PENDING_STOP

  if (steps == (UDIUInt32) 0)
    return (UDINoError);

  if ((steptype & UDIStepOverCalls) || (steptype & UDIStepOverTraps) ||
       (steptype & UDIStepInRange))
       return (UDIErrorUnsupportedStepType);

  StepCmdGiven = 1;
  Mini_build_step_msg(steps);
  if (Mini_msg_send() != SUCCESS)
    return ((-1) * MONErrCantSendMsg);
  RefreshRegs = 1;
  ProcessorState = (UDIUInt32) UDIRunning;
  PreviousProcessorState = (UDIUInt32) UDIRunning;

  return (UDINoError);
}

UDIVoid 
UDIStop()
{
  int	GrossState;

  GrossState = (int) (ProcessorState & 0xFF);
  if ((GrossState == UDINotExecuting) || (GrossState == UDIRunning) ||
      (GrossState == UDIStdoutReady) || (GrossState == UDIStderrReady) ||
      (GrossState == UDIStdinNeeded) ) {
      StopFlag = 1; /* This will be reset after its handled */
  }
  /* Else ignored */
  return; 
}

UDIError 
UDIWait(maxtime, pid, stop_reason)
  UDIInt32        maxtime;
  UDIPId         *pid;
  UDIUInt32      *stop_reason;
{
  INT32           MsgCode;

  *pid = (UDIPId) CurrentPID;

  if (ProcessorState == (UDIUInt32) UDIRunning) {
    while (1) { /* handle messages as long as they are coming */
      if (MsgAlreadyInBuffer==1) {
	MsgCode = MsgAlreadyReceived;
	MsgAlreadyInBuffer=0;
      } else {
        MsgCode = CheckForMsg(maxtime);
      }
#if 0
      MsgCode = CheckForMsg(maxtime);
#endif
      if ((MsgCode == FAILURE) || (MsgCode == ABORT_FAILURE)) { /* no news */
        *stop_reason = ProcessorState;	
        return (UDINoError);
      } else {	/* a message from target has arrived */
        *stop_reason = ProcessorState;
	if (MsgCode == CHANNEL0_ACK) {
	   process_chan0_ack();
	   return (UDINoError);
	}
        (void) process_target_msg(MsgCode);
        if (ProcessorState != (UDIUInt32) UDIRunning) 
          return (UDINoError);
      };
    }
  } else {
    *stop_reason = ProcessorState;
    return (UDINoError);
  }
}

UDIError 
UDISetBreakpoint(addr, pass_count, bk_type, break_id)
  UDIResource     addr;
  UDIInt32        pass_count;
  UDIBreakType    bk_type;
  BreakIdType    *break_id;
{
  INT32           space = SpaceMap_udi2mm(addr.Space);
  ADDR32          ack_addr;
  INT32           set_count,
                  set_type;
  BreakIdType     newid;
  UDIUInt32       BreakInst;	/* EB29K */
  UDIError        UDIretval;	/* EB29K */

  CLEAR_PENDING_STOP

  if (space < 0)
    return (UDIErrorUnknownResourceSpace);

  /*
   * Minimon currently supports only two types of breakpoints * BKPT_29000
   * and BKPT_29050 *
   */
  if (bk_type & MONBreakFlagHardware) {
    if (bk_type & MONBreakTranslationEnabled)
      bk_type = BKPT_29050_BTE_1;
    else
      bk_type = BKPT_29050_BTE_0;	/* default */
  } else if ((bk_type & UDIBreakFlagRead) || (bk_type & UDIBreakFlagWrite))
    return ((-1) * MONErrUnknownBreakType);
  else if (bk_type & UDIBreakFlagExecute)
    bk_type = BKPT_29000;	/* Minimon uses this */

  if (pass_count == (UDIInt32) 0)
     pass_count = (UDIInt32) -1; /* make it temporary */
  Mini_build_bkpt_set_msg(space,
			  (ADDR32) addr.Offset,
			  (INT32) pass_count,
			  (INT32) bk_type);
  SEND_AND_WAIT_ACK (BKPT_SET_ACK);
  if (ReturnedError == (int) 1) {
    ReturnedError = 0;
    return ((-1) * MONErrErrorSetBreak);
  }
  Mini_unpack_bkpt_set_ack_msg((INT32 *) &space,
			       (ADDR32 *) &ack_addr,
			       (INT32 *) &set_count,
			       (INT32 *) &set_type);

  BreakInst = (ADDR32) - 1;

  if (!strcmp(TargetType, "eb29k")) {	/* For EB29K */
    /* UDIRead(); read instruction */
    if ((UDIretval = UDIRead(addr,
			     (UDIHostMemPtr) &BreakInst,
			     (UDICount) 4,
			     (UDISizeT) 1,
			     (UDICount *) &ack_addr,
			     (UDIBool) FALSE)) != UDINoError)	/* 29K endian */
      return (UDIretval);
  };

  add_to_bp_table(&newid, space, addr.Offset, set_count, set_type, BreakInst);
  *break_id = (BreakIdType) newid;
  LastBreakId = newid + 1;	/* ??? */
  return (UDINoError);
}

UDIError 
UDIQueryBreakpoint(break_id, addr, pass_count,
		   bk_type, current_count)
  BreakIdType     break_id;
  UDIResource    *addr;
  UDIInt32       *pass_count;
  UDIBreakType   *bk_type;
  UDIInt32       *current_count;
{
  INT32           space;
  ADDR32          offset;
  INT32           pcount;
  INT32           type;
  INT32           ccount;
  ADDR32          Inst;

  CLEAR_PENDING_STOP

  if (break_id >= LastBreakId)
    return (UDIErrorNoMoreBreakIds);

  if (get_from_bp_table(break_id, &space, &offset, &pcount, &type, &Inst) != 0)
    return (UDIErrorInvalidBreakId);

  Mini_build_bkpt_stat_msg(space, offset);
  SEND_AND_WAIT_ACK (BKPT_STAT_ACK);
  if (ReturnedError == (int) 1) {
    ReturnedError = 0;
    return ((-1) * MONErrErrorStatBreak);
  }
  Mini_unpack_bkpt_stat_ack_msg((INT32 *) &space,
				(ADDR32 *) &offset,
				(INT32 *) &ccount,
				(INT32 *) &type);

  addr->Space = SpaceMap_mm2udi(space);
  addr->Offset = (CPUOffset) offset;
  *pass_count = (UDIInt32) pcount;

  if (type == (INT32) BKPT_29000)
    type = UDIBreakFlagExecute;
  else if (type == BKPT_29050_BTE_0)
    type = (MONBreakFlagHardware | UDIBreakFlagExecute);
  else if (type == BKPT_29050_BTE_1)
    type = (MONBreakTranslationEnabled | MONBreakFlagHardware | UDIBreakFlagExecute);
  *bk_type = (UDIBreakType) type;

  *current_count = (UDIInt32) ccount;

  return (UDINoError);
}

UDIError 
UDIClearBreakpoint(break_id)
  BreakIdType     break_id;
{
  INT32           space;
  ADDR32          offset;
  INT32           count;
  INT32           type;
  ADDR32          Inst;
  UDIResource     addr;	/* EB29K */
  UDIError        UDIretval;	/* EB29K */

  CLEAR_PENDING_STOP

  /* should bkpt be removed from linked list ?? */
  if (get_from_bp_table(break_id, &space, &offset, &count, &type, &Inst) != 0)
    return (UDIErrorInvalidBreakId);

  Mini_build_bkpt_rm_msg(space, offset);
  SEND_AND_WAIT_ACK (BKPT_RM_ACK);
  if (ReturnedError == (int) 1) {
    ReturnedError = 0;
    return ((-1) * MONErrErrorRmBreak);
  }
  Mini_unpack_bkpt_rm_ack_msg(&space, &offset);

  if (!strcmp(TargetType, "eb29k")) {	/* For EB29K */
    /* Write back the original instruction * UDIWrite(Inst); */
    addr.Offset = offset;
    addr.Space = SpaceMap_mm2udi(space);
    if ((UDIretval = UDIWrite((UDIHostMemPtr) &Inst,
			      addr,
			      (UDICount) 4,
			      (UDISizeT) 1,
			      &offset,
			      FALSE)) != UDINoError)
      return (UDIretval);
  };

  remove_from_bp_table(break_id);

  return (UDINoError);
}

UDIError 
UDIGetStdout(buf, bufsize, count_done)
  UDIHostMemPtr   buf;
  UDISizeT          bufsize;
  UDISizeT         *count_done;
{
  static int      chan1_indx = 0;
  UDISizeT          mincount;
  UDISizeT          i;
  char           *temp;
  UDIUInt32	  reg_val;
  UDIError	  UDIretval;


  if ((int) (ProcessorState & 0xFF) != (int) UDIStdoutReady) {
     *count_done = (UDISizeT) 0;
      return (UDINoError);
  };

  temp = (char *) buf;	/* used for copying */
  i = (UDISizeT) chan1_indx;
  if (Channel1_count) {
    mincount = (Channel1_count < (UDISizeT) bufsize) ? Channel1_count :
	(UDISizeT) bufsize;
    for (i = 0; i < mincount; i++) {
      (char) *temp++ = (char) channel1_buffer[chan1_indx];
      chan1_indx = (chan1_indx + 1) % TIP_IO_BUFSIZE;	/* circular buffer */
    }
    *count_done = (UDISizeT) mincount;
    Channel1_count = Channel1_count - mincount;
    TotalDone = TotalDone + (UDIUInt32) mincount;
    if (Channel1_count <= (UDISizeT) 0) {
	/*
	 * The HIF kernel from MiniMON29K release 2.1 expects MONTIP
	 * to send a HIF_CALL_RTN response for a HIF_CALL message, and
	 * a CHANNEL1_ACK response for a CHANNEL1 message, and 
	 * a CHANNEL2_ACK response for a CHANNEL2 message, and
	 * a CHANNEL0 message for a asynchronous input.
	 * The HIF kernel version numbers 0x05 and above support these
	 * features.
	 */
     if ((tip_target_config.os_version & 0xf) > 4) { /* new HIF kernel */
       if (!NoChan1Ack) {
          Mini_build_channel1_ack_msg(TotalDone); /* send gr96 value */
          if (Mini_msg_send() != SUCCESS)
            return ((-1) * MONErrCantSendMsg);
       }
     } else { /* old HIF kernel */
       if ((UDIretval = Write_Glob_Reg(TotalDone, (int) 96)) != UDINoError)
         return (UDIretval);
       reg_val = (UDIUInt32) 0x80000000;
       if ((UDIretval = Write_Glob_Reg(reg_val, (int) 121)) != UDINoError)
         return (UDIretval);
     }
      TotalDone = (UDIUInt32) 0;
      Channel1_count = (UDISizeT) 0;
      chan1_indx = 0;
    } else {
      return (UDINoError);
    }
  } else {
    *count_done = (UDISizeT) 0;
    TotalDone = (UDIUInt32) 0;
    Channel1_count = (UDISizeT) 0;
    chan1_indx = 0;
	/*
	 * The HIF kernel from MiniMON29K release 2.1 expects MONTIP
	 * to send a HIF_CALL_RTN response for a HIF_CALL message, and
	 * a CHANNEL1_ACK response for a CHANNEL1 message, and 
	 * a CHANNEL2_ACK response for a CHANNEL2 message, and
	 * a CHANNEL0 message for a asynchronous input.
	 * The HIF kernel version numbers 0x05 and above support these
	 * features.
	 */
     if ((tip_target_config.os_version & 0xf) > 4) { /* new HIF kernel */
       Mini_build_channel1_ack_msg(TotalDone); /* send gr96 value */
       if (Mini_msg_send() != SUCCESS)
         return ((-1) * MONErrCantSendMsg);
     } else { /* old HIF kernel */
       if ((UDIretval = Write_Glob_Reg(TotalDone, (int) 96)) != UDINoError)
         return (UDIretval);
       reg_val = (UDIUInt32) 0x80000000;
       if ((UDIretval = Write_Glob_Reg(reg_val, (int) 121)) != UDINoError)
         return (UDIretval);
     }
  }
  if (StepCmdGiven) {
    ProcessorState = UDIStepped;
    PreviousProcessorState = UDIStepped;
    StepCmdGiven = 0;
  } else {
      if (!BreaksInPlace) {
	PutAllBreakpoints();
	BreaksInPlace = 1;
      }
	/*
	 * The HIF kernel from MiniMON29K release 2.1 expects MONTIP
	 * to send a HIF_CALL_RTN response for a HIF_CALL message, and
	 * a CHANNEL1_ACK response for a CHANNEL1 message, and 
	 * a CHANNEL2_ACK response for a CHANNEL2 message, and
	 * a CHANNEL0 message for a asynchronous input.
	 * The HIF kernel version numbers 0x05 and above support these
	 * features.
	 */
     if ((tip_target_config.os_version & 0xf) > 4) { /* new HIF kernel */
       ProcessorState = (UDIUInt32) UDIRunning;
       PreviousProcessorState = (UDIUInt32) UDIRunning;
     } else { /* old HIF kernel */
       UDIExecute();	/* sends a GO to the Debugger to start application */
     }
  }
  return (UDINoError);
}

#ifdef	MSDOS
UDIError OldHIFGetStderr( UDIHostMemPtr buf,UDISizeT bufsize,UDISizeT *count_done);
#else
UDIError OldHIFGetStderr();
#endif

UDIError 
UDIGetStderr(buf, bufsize, count_done)
  UDIHostMemPtr   buf;
  UDISizeT          bufsize;
  UDISizeT         *count_done;
{
  static int      chan2_indx = 0;
  UDISizeT          mincount;
  UDISizeT          i;
  char           *temp;

  if ((int) (ProcessorState & 0xFF) != (int) UDIStderrReady) {
    *count_done = (UDISizeT) 0;
    return (UDINoError);
  };
	/*
	 * The HIF kernel from MiniMON29K release 2.1 expects MONTIP
	 * to send a HIF_CALL_RTN response for a HIF_CALL message, and
	 * a CHANNEL1_ACK response for a CHANNEL1 message, and 
	 * a CHANNEL2_ACK response for a CHANNEL2 message, and
	 * a CHANNEL0 message for a asynchronous input.
	 * The HIF kernel version numbers 0x05 and above support these
	 * features.
	 */
     if ((tip_target_config.os_version & 0xf) > 4) { /* new HIF kernel */
	/*
	 * From MiniMON29K release 2.1 all interactions with 
	 * stdin, stdout, stderr by the application is handled without
	 * invoking the Debugger on the target. Thus, a write to stderr
	 * is implemented by a CHANNEL2 message similar to the CHANNEL1
	 * message for stdout.
	 */
       temp = (char *) buf;	/* used for copying */
       i = (UDISizeT) chan2_indx;
       if (Channel2_count) {
         mincount = (Channel2_count < (UDISizeT) bufsize) ? Channel2_count :
	     (UDISizeT) bufsize;
         for (i = 0; i < mincount; i++) {
           (char) *temp++ = (char) channel2_buffer[chan2_indx];
           chan2_indx = (chan2_indx + 1) % TIP_IO_BUFSIZE;/* circular buffer */
         }
         *count_done = (UDISizeT) mincount;
         Channel2_count = Channel2_count - mincount;
         TotalDone = TotalDone + (UDIUInt32) mincount;
         if (Channel2_count <= (UDISizeT) 0) {
            Mini_build_channel2_ack_msg(TotalDone); /* send gr96 value */
            if (Mini_msg_send() != SUCCESS)
              return ((-1) * MONErrCantSendMsg);
            TotalDone = (UDIUInt32) 0;
            Channel2_count = (UDISizeT) 0;
            chan2_indx = 0;
          } else {
            return (UDINoError);
          }
       } else {
          *count_done = (UDISizeT) 0;
          TotalDone = (UDIUInt32) 0;
          Channel2_count = (UDISizeT) 0;
          chan2_indx = 0;
          Mini_build_channel2_ack_msg(TotalDone); /* send gr96 value */
          if (Mini_msg_send() != SUCCESS)
            return ((-1) * MONErrCantSendMsg);
      }
      if (StepCmdGiven) {
        ProcessorState = UDIStepped;
        PreviousProcessorState = UDIStepped;
        StepCmdGiven = 0;
      } else {
          if (!BreaksInPlace) {
             PutAllBreakpoints();
             BreaksInPlace = 1;
          }
        ProcessorState = (UDIUInt32) UDIRunning;
        PreviousProcessorState = (UDIUInt32) UDIRunning;
      }
     } else { /* old HIF kernel code */
	return (OldHIFGetStderr(buf, bufsize, count_done));
     }	/* old kernel code */

  return (UDINoError);
}

UDIError
OldHIFGetStderr(buf, bufsize, count_done)
  UDIHostMemPtr   buf;
  UDISizeT          bufsize;
  UDISizeT         *count_done;
/*
 * For HIF kernel version 0x04 and lower.
 */
{
  UDIUInt32       count;
  UDIUInt32       done;
  UDIResource     from;
  UDIBool         host_endian;
  UDISizeT          size;
  UDIError        UDIretval;
  UDIUInt32       reg_val;

       /* Lr4_count gives the bytes to be written */
       /* Lr3_addr gives the address in the target */
       if (Lr4_count > (UDIUInt32) 0) {
         count = (Lr4_count < (UDIUInt32) bufsize) ? Lr4_count : (UDIUInt32) bufsize;
         /* read count bytes from Lr3_addr */
         from.Offset = Lr3_addr;
         from.Space = UDI29KDRAMSpace;
         size = 1;
         host_endian = FALSE;
         if ((UDIretval = UDIRead(from,
			          buf,
			          count,
			          size,
			          &done,
			          host_endian)) != UDINoError) {
           return (UDIretval);
         }
         *count_done = (UDISizeT) count;
         Lr4_count = Lr4_count - count;
         Lr3_addr = Lr3_addr + count;
         TotalDone = TotalDone + (UDIUInt32) count;
         if (Lr4_count <= (UDISizeT) 0) {
             if ((UDIretval = Write_Glob_Reg(TotalDone, (int) 96)) != UDINoError)
                  return (UDIretval);
             reg_val = (UDIUInt32) 0x80000000;
             if ((UDIretval = Write_Glob_Reg(reg_val, (int) 121)) != UDINoError)
                  return (UDIretval);
             TotalDone = (UDIUInt32) 0;
	     Lr4_count = (UDIUInt32) 0;
         } else {
           return (UDINoError);
         }
       } else {
         *count_done = (UDISizeT) 0;
         TotalDone = (UDIUInt32) 0;
         if ((UDIretval = Write_Glob_Reg(TotalDone, (int) 96)) != UDINoError)
              return (UDIretval);
         reg_val = (UDIUInt32) 0x80000000;
         if ((UDIretval = Write_Glob_Reg(reg_val, (int) 121)) != UDINoError)
              return (UDIretval);
         Lr4_count = (UDIUInt32) 0;
       };
     
       /* Resume execution  UDIExecute()? */
       if (StepCmdGiven) {
         ProcessorState = UDIStepped;
         PreviousProcessorState = UDIStepped;
         StepCmdGiven = 0;
       } else {
           if (!BreaksInPlace) {
	     PutAllBreakpoints();
	     BreaksInPlace = 1;
           }
         UDIExecute();
       }
  return (UDINoError);
}

#ifdef MSDOS
UDIError OldHIFPutStdin( UDIHostMemPtr buf,UDISizeT count,UDISizeT *count_done);
#else
UDIError OldHIFPutStdin();
#endif

UDIError 
UDIPutStdin(buf, count, count_done)
  UDIHostMemPtr   buf;
  UDISizeT          count;
  UDISizeT         *count_done;
{
     char	*CharPtr;
     UINT32	MinCnt;
     INT32	Code;

     if ((tip_target_config.os_version & 0xf) > 0x6) { /* MiniMON29K 3.0 */
       if ((int) (ProcessorState & 0xFF) != (int) UDIStdinNeeded) {
           /* non-blocking mode asynchronous mode */
	   /* 
	    if asynchronous mode, we sent a channel0 message for
	    every character sent by DFE in this call. 
	    */
	    if (PgmStdinMode & TIP_NBLOCK) { 
               if ((Code = Mini_msg_recv(NONBLOCK)) != FAILURE) {
		   MsgAlreadyReceived=Code;
		   MsgAlreadyInBuffer = 1;
	       } 
               CharPtr = buf;
	       if (!Channel0Busy) {
	         /* send one character and return to DFE */
	         Mini_build_channel0_msg(CharPtr, (INT32) 1);
	         /*
		  * Just send the message here, and wait for the ack later
	         SEND_AND_WAIT_ACK(CHANNEL0_ACK);
	         */
	         if (Mini_msg_send() != SUCCESS) 
		    return((-1) * MONErrCantSendMsg);
#if 0
	         Channel0Busy = 1;	/* never set */
#endif
	       } else {
		 /* save it in channel0_buffer */
	         channel0_buffer[Channel0_count] = (char) *CharPtr;
	         Channel0_count=Channel0_count+1;
	       }
	       *count_done = (UDISizeT) 1;
	       return (UDINoError);
	    } else  if (PgmStdinMode & TIP_ASYNC) {
               if ((Code = Mini_msg_recv(NONBLOCK)) != FAILURE) {
		   MsgAlreadyReceived=Code;
		   MsgAlreadyInBuffer = 1; /* check in UDIWait */
	       } 
               CharPtr = buf;
	       *count_done = (UDISizeT) 0;
	       for ( ; count > 0; count--) {
	            Mini_build_channel0_msg(CharPtr, (INT32) 1);
	           /*
		    * Just send the message here, and wait for the ack later
	           SEND_AND_WAIT_ACK(CHANNEL0_ACK);
	           */
		    if (Mini_msg_send() != SUCCESS)
		      return((-1)*MONErrCantSendMsg);
	            *count_done = (UDISizeT) (*count_done + (UDISizeT) 1);
	            CharPtr++;
	       }
    	       return (UDINoError);
	    }
       } else { /* synchronous mode */
	  /*
	   in synchronous mode, we send all the characters received using
	   stdin_needed_ack_msg, when the processorstate becomes stdinneeded.
	   This is line-buffered mode. So we clear variables
	   after we send.
	   What do we do when DFE sends more characters than we need now?
	   The count_done return value gives number accepted. But who keeps
	   the rest.
	   Otherwise, what do we do???????
	   */
	  if (PgmStdinMode & TIP_NBLOCK) { 
	     /* send one character and return to DFE */
             CharPtr = buf;
             if ((Code = Mini_msg_recv(NONBLOCK)) != FAILURE) {
		   MsgAlreadyReceived=Code;
		   MsgAlreadyInBuffer = 1;
	     }
	     *count_done = (UDISizeT) 1;
	     Mini_build_channel0_msg(CharPtr, (INT32) 1);
	     /*
	      * Send the message now and wait for the ack later.
	     SEND_AND_WAIT_ACK(CHANNEL0_ACK);
	      */
	     if (Mini_msg_send() != SUCCESS)
	       return ((-1)*MONErrCantSendMsg);
	     return (UDINoError);
	  }; 

	  MinCnt = ((UINT32) StdinCharsNeeded > (UINT32) count) ?
		      (UINT32) count : (UINT32) StdinCharsNeeded;
	  Mini_build_stdin_needed_ack_msg (MinCnt, buf);
          if (Mini_msg_send() != SUCCESS)
            return ((-1) * MONErrCantSendMsg);
	  *count_done = (UDISizeT) MinCnt;
	  StdinCharsNeeded = 0;		/* reset to zero ?? */
          if (StepCmdGiven) {
            ProcessorState = UDIStepped;
            PreviousProcessorState = UDIStepped;
            StepCmdGiven = 0;
          } else {
            if (!BreaksInPlace) {
	      PutAllBreakpoints();
	      BreaksInPlace = 1;
            }
	    ProcessorState = UDIRunning;
	    PreviousProcessorState = UDIRunning;
	  }
	  return (UDINoError);
       }
     } else if ((tip_target_config.os_version & 0xf) > 4) { /* pre-release */
	/*
	 * The HIF kernel from MiniMON29K release 2.1 expects MONTIP
	 * to send a HIF_CALL_RTN response for a HIF_CALL message, and
	 * a CHANNEL1_ACK response for a CHANNEL1 message, and 
	 * a CHANNEL2_ACK response for a CHANNEL2 message, and
	 * a CHANNEL0 message for a asynchronous input.
	 * The HIF kernel version numbers 0x05 and above support these
	 * features.
	 */
       /* Send CHANNEL0 message depending on StdinMode. */
       CharPtr = buf;
       if (PgmStdinMode == TIP_COOKED) { /* default line buffered */
	 /*
	  * send a line of input using channel0 
	  * Check for '\n' sent from DFE.
	  */
	  if ((int) *CharPtr == (int) 8) {/* backspace */
	    Channel0_count=Channel0_count-1;
	  } else if ((int) *CharPtr == (int) 127) {/* delete */
	    Channel0_count=Channel0_count-1;
#ifdef MSDOS
	  } else if ((int) *CharPtr == (int) 10) {/* \n */
	    /* simply return, no change. already padded. */
	    *count_done = count;
	    return (UDINoError);
	  } else if ((int) *CharPtr == (int) 13) {/* end of line */
	    channel0_buffer[Channel0_count] = (char) *CharPtr;
	    Channel0_count=Channel0_count+1;
	    channel0_buffer[Channel0_count] = (char) 10; /* add \n */
	    Channel0_count=Channel0_count+1;
	    Mini_build_channel0_msg(channel0_buffer, Channel0_count);
	    SEND_AND_WAIT_ACK(CHANNEL0_ACK);
	    Channel0_count = 0;	/* reset */
	    *count_done = count;
#else	/* MSDOS */
	  } else if ((int) *CharPtr == (int) 13) {/* end of line */
	    /* simply return, added on \n */
	    *count_done = count;
	    return (UDINoError);
	  } else if ((int) *CharPtr == (int) 10) {/* \n */
	    channel0_buffer[Channel0_count] = (char) 13; /* add \r */
	    Channel0_count=Channel0_count+1;
	    channel0_buffer[Channel0_count] = (char) *CharPtr;
	    Channel0_count=Channel0_count+1;
	    Mini_build_channel0_msg(channel0_buffer, Channel0_count);
	    SEND_AND_WAIT_ACK(CHANNEL0_ACK);
	    Channel0_count = 0;	/* reset */
#endif /* MSDOS */
	  } else { /* store it in buffer here */
	    channel0_buffer[Channel0_count] = (char) *CharPtr;
	    Channel0_count=Channel0_count+1;
	    *count_done = count;
	  }
	  return (UDINoError);
       } else if (PgmStdinMode == TIP_RAW) { /* for other modes of input */
	    channel0_buffer[Channel0_count] = (char) *CharPtr;
	    Channel0_count=Channel0_count+1;
	    Mini_build_channel0_msg(channel0_buffer, Channel0_count);
	    SEND_AND_WAIT_ACK(CHANNEL0_ACK);
	    Channel0_count = 0;	/* reset */
	    *count_done = count;
	    return (UDINoError);
       } else { /* for other modes of input */
	 /* NOT IMPLEMENTED */
	    return (UDINoError);
       }
     } else { /* old HIF kernel */
       return(OldHIFPutStdin(buf, count, count_done));
     }
}

UDIError
OldHIFPutStdin(buf, count, count_done)
  UDIHostMemPtr   buf;
  UDISizeT          count;
  UDISizeT         *count_done;
{
  UDIResource     to;
  UDIError        retval;
  UDIBool         hostendian;
  UDICount        mincount,
                  bytes_ret;
  UDIUInt32       reg_val;
  UDISizeT          size;

  if ((int) (ProcessorState & 0xFF) != (int) UDIStdinNeeded) {
    *count_done = (UDISizeT) 0;
    return (UDINoError);
  };
  /* Lr4_count has count requested */
  /* Lr3_addr has the destination */
  if (Lr4_count > (UDIUInt32) 0) {
    mincount = ((UDICount) count < (UDICount) Lr4_count) ?
	(UDICount) count :
	(UDICount) Lr4_count;
    to.Space = (CPUSpace) UDI29KDRAMSpace;
    to.Offset = (CPUOffset) Lr3_addr;
    size = (UDISizeT) 1;
    hostendian = FALSE;
    if ((retval = UDIWrite(buf,
			   to,
			   mincount,
			   size,
			   &bytes_ret,
			   hostendian)) != UDINoError) {
      return ((UDIError) retval);
    };
    Lr4_count = (UDIUInt32) 0;
    *count_done = (UDISizeT) bytes_ret;
  } else {
    Lr4_count = (UDIUInt32) 0;
    *count_done = (UDISizeT) 0;
  };

  /*
   * ASSUMPTION: It's always a non-blocking read & this function is called
   * only when app. needs data. So, write the number of bytes read to gr96 on
   * the target.
   */
  /* Write gr96  set above */
  /* gr96 */
  reg_val = (UDIUInt32) * count_done;	/* same as mincount */
  if ((retval = Write_Glob_Reg(reg_val, (int) 96)) != UDINoError)
    return (retval);

  /* Write Gr121 */
  reg_val = (UDIUInt32) 0x80000000;
  if ((retval = Write_Glob_Reg(reg_val, (int) 121)) != UDINoError)
    return (retval);

  if (StopFlag) {
     STOP_SIG_HDLR
     ProcessorState = (UDIUInt32) UDIStopped;
     PreviousProcessorState = (UDIUInt32) UDIStopped;
     return (UDINoError);
  }; 
  /* Resume execution UDIExecute()? */
  if (StepCmdGiven) {
    ProcessorState = UDIStepped;
    PreviousProcessorState = UDIStepped;
    StepCmdGiven = 0;
  } else {
      if (!BreaksInPlace) {
	PutAllBreakpoints();
	BreaksInPlace = 1;
      }
    UDIExecute();
  }

  return (UDINoError);
}

UDIError 
UDIStdinMode(mode)
 UDIMode   *mode;
{
  *mode = (UDIMode) (PgmStdinMode);
  /* restore ProcessorState from saved value in PreviousState */
  ProcessorState = PreviousProcessorState;
  return (UDINoError);
}

UDIError 
UDIPutTrans(buf, count, count_done)
  UDIHostMemPtr   buf;
  UDISizeT          count;
  UDISizeT         *count_done;
{
  char	*tip_token;

  tip_token = strtok ((char *) buf, " \t,;\n\r");
  if (tip_token == NULL)
        return ((-1) * MONErrUnknownTIPCmd);

  if (strcmp (tip_token, "tip") == 0)  {
    tip_token = strtok ((char *) 0, " \t,;\n\r");
    if (tip_token == NULL)
       return ((-1) * MONErrUnknownTIPCmd);
    else {
      if (strcmp(tip_token, "lpt=1") == 0) {
#ifdef MSDOS
	set_lpt();
#endif
	use_parport = 1;
      } else if (strcmp(tip_token, "lpt=0") == 0) {
#ifdef MSDOS
	unset_lpt();
#endif
	use_parport = 0;
      } else
       return ((-1) * MONErrUnknownTIPCmd);
    }
    return (UDINoError);
  } else {
    return ((-1) * MONErrUnknownTIPCmd);
  }

}

UDIError 
UDIGetTrans(buf, bufsize, count)
  UDIHostMemPtr   buf;
  UDISizeT          bufsize;
  UDISizeT         *count;
{
  CLEAR_PENDING_STOP
  return (UDIErrorUnsupportedService);
}

UDIError 
UDITransMode(mode)
  UDIMode   *mode;
{
  CLEAR_PENDING_STOP
  return (UDIErrorUnsupportedService);
}

/* Map Space conversion functions */

static INT32 
SpaceMap_udi2mm(space)
  CPUSpace        space;
{
  switch (space) {
   case UDI29KDRAMSpace:
    return ((INT32) D_MEM);
   case UDI29KIOSpace:
    return ((INT32) I_O);
   case UDI29KCPSpace0:
    return ((INT32) SPECIAL_REG);
   case UDI29KCPSpace1:
    return ((INT32) SPECIAL_REG);
   case UDI29KIROMSpace:
    return ((INT32) I_ROM);
   case UDI29KIRAMSpace:
    return ((INT32) I_MEM);
   case UDI29KLocalRegs:
    return ((INT32) LOCAL_REG);
   case UDI29KGlobalRegs:
    return ((INT32) GLOBAL_REG);
   case UDI29KRealRegs:
    return ((INT32) GLOBAL_REG);
   case UDI29KSpecialRegs:
    return ((INT32) SPECIAL_REG);
   case UDI29KTLBRegs:
    return ((INT32) TLB_REG);
   case UDI29KACCRegs:
    return ((INT32) SPECIAL_REG);
   case UDI29KICacheSpace:
    return ((INT32) I_CACHE);
   case UDI29KAm29027Regs:
    return ((INT32) COPROC_REG);
   case UDI29KPC:
    return ((INT32) PC_SPACE);
   case UDI29KDCacheSpace:
    return ((INT32) D_CACHE);
   case VERSION_SPACE:
    return ((INT32) VERSION_SPACE);
   default:
    return (FAILURE);
  };
}

static CPUSpace 
SpaceMap_mm2udi(space)
  INT32           space;
{
  switch (space) {
   case LOCAL_REG:
    return ((CPUSpace) UDI29KLocalRegs);
   case ABSOLUTE_REG:
    return ((CPUSpace) UDI29KGlobalRegs);
   case GLOBAL_REG:
    return ((CPUSpace) UDI29KGlobalRegs);
   case SPECIAL_REG:
    return ((CPUSpace) UDI29KSpecialRegs);
   case TLB_REG:
    return ((CPUSpace) UDI29KTLBRegs);
   case COPROC_REG:
    return ((CPUSpace) UDI29KAm29027Regs);
   case I_MEM:
    return ((CPUSpace) UDI29KIRAMSpace);
   case D_MEM:
    return ((CPUSpace) UDI29KDRAMSpace);
   case I_ROM:
    return ((CPUSpace) UDI29KIROMSpace);
   case D_ROM:
    return ((CPUSpace) UDI29KIROMSpace);
   case I_O:
    return ((CPUSpace) UDI29KIOSpace);
   case I_CACHE:
    return ((CPUSpace) UDI29KICacheSpace);
   case D_CACHE:
    return ((CPUSpace) UDI29KDCacheSpace);
   case PC_SPACE:
    return ((CPUSpace) UDI29KPC);
   case A_SPCL_REG:
    return ((CPUSpace) UDI29KSpecialRegs);
   default:
    return (FAILURE);
  }
}

/* Miscellaneous UDI support functions */

static int
Reset_Processor()
{
  INT32           MsgCode;
  BreakIdType		i;

  CLEAR_PENDING_STOP

  Mini_build_reset_msg();
  if (Mini_msg_send() != SUCCESS)
    return ((-1) * MONErrCantSendMsg);

  MsgCode = Wait_For_Ack();
  if (MsgCode == ABORT_FAILURE)
     return (UDIErrorAborted);
  else if (MsgCode == FAILURE)
	return ((-1) * MONErrNoSynch);

  RefreshRegs = 1;
  /* Clear all breakpoints */
  BreaksInPlace = 0;
  for (i = 1; i < LastBreakId; i++)
     remove_from_bp_table(i);

  return (SUCCESS);
}

static int 
parse_string(string)
  char           *string;
{
  char           *s;

  if ((string == NULL) || (strcmp(string,"") == 0))
    return ((-1) * MONErrNullConfigString);

  (void) strcpy (&ConnectString[0], string); /* to preserve the original */

  s = strtok(ConnectString, " ");

  while (s != NULL) {
    if ((s[0] == '-') && (s[1] != '\0') && (s[2] == '\0')) {	/* single letter options */
      switch (s[1]) {
       case 'S':	/* -Supervisor Mode */
	SupervisorMode = 1;	/* always in real mode */
	RealMode = 1;
	ProtectedMode = 0;
	VirtualMode = 0;
	s = strtok(NULL, " ");	/* get next string */
	break;
       case 'R':	/* -Real Mode */
	RealMode = 1;
	ProtectedMode = 0;
	VirtualMode = 0;
	s = strtok(NULL, " ");	/* get next string */
	break;
       case 'P':	/* _Protected mode */
	SupervisorMode = 0;	/* SM mode not supported */
	RealMode = 0;
	VirtualMode = 0;
	ProtectedMode = 1;
	s = strtok(NULL, " ");	/* get next string */
	break;
       case 'V':	/* -Virtual mode */
	SupervisorMode = 0;	/* SM mode not supported */
	RealMode = 0;
	ProtectedMode = 0;
	VirtualMode = 1;
	s = strtok(NULL, " ");	/* get next string */
	break;
       case 'r':	/* core file */
	s = strtok(NULL, " ");
	if (s == NULL) {	/* error */
	  return (UDIErrorInvalidTIPOption);	/* UNKNOWN */
	} else {
	  if ((CoreFile = (char *) malloc(strlen(s) + 1)) == NULL) {
	    return ((-1) * MONErrOutofMemory);	/* EMALLOC ? */
	  };
	  (void) strcpy(CoreFile, s);
	};
	s = strtok(NULL, " ");	/* get next string */
	break;
       case 't':	/* target type */
	s = strtok(NULL, " ");
	if (s == NULL) {	/* error */
	  return (UDIErrorInvalidTIPOption);	/* UNKNOWN */
	} else {
	  if ((TargetType = (char *) malloc(strlen(s) + 1))
	      == NULL) {
	    return ((-1) * MONErrOutofMemory);	/* EMALLOC ? */
	  };
	  (void) strcpy(TargetType, s);
	};
	s = strtok(NULL, " ");	/* get next string */
	break;
       case 'm':	/* message log file */
	s = strtok(NULL, " ");
	if (s == NULL) {	/* error */
	  return (UDIErrorInvalidTIPOption);	/* UNKNOWN */
	} else {
	  if ((Msg_Logfile = (char *) malloc(strlen(s) + 1)) == NULL) {
	    return ((-1) * MONErrOutofMemory);	/* EMALLOC ? */
	  };
	  (void) strcpy(Msg_Logfile, s);
	};
	s = strtok(NULL, " ");	/* get next string */
	break;
       default:	/* unknown */
	return (UDIErrorInvalidTIPOption);	/* UNKNOWN */
      };	/* end switch */
    } else {	/* multiple letter options */
      if (strcmp(s, "-com") == 0) {
	s = strtok(NULL, " ");
	if (s == NULL) {
	  return (UDIErrorInvalidTIPOption);
	} else {
	  (void) strcpy(tip_config.comm_port, s);
	};
	s = strtok(NULL, " ");	/* get next string */
      } else if (strcmp(s, "-af") == 0) {
	SendACKFirst = 1;
	s = strtok(NULL, " ");	/* get next string */
      } else if (strcmp(s, "-par") == 0) {
	s = strtok(NULL, " ");
	if (s == NULL) {
	  return (UDIErrorInvalidTIPOption);
	} else {
	  (void) strcpy(tip_config.par_port, s);
	  lpt_initialize = 1;
	};
	s = strtok(NULL, " ");	/* get next string */
      } else if (strcmp(s, "-le") == 0) { /* little endian target */
	tip_target_config.P29KEndian = LITTLE;
	s = strtok(NULL, " ");	/* get next string */
      } else if (strcmp(s, "-re") == 0) {
	s = strtok(NULL, " ");
	if (s == NULL) {	/* error */
	  return (UDIErrorInvalidTIPOption);	/* UNKNOWN */
	} else {
	  if (sscanf(s, "%d", &MessageRetries) != 1)
	    return (UDIErrorInvalidTIPOption);
	};
	s = strtok(NULL, " ");	/* get next string */
      } else if (strcmp(s, "-na") == 0) { /* no need to ack channel1 msg */
	NoChan1Ack = 1;
	s = strtok(NULL, " ");	/* get next string */
      } else if (strcmp(s, "-nt") == 0) {
	NoStepReqd = 1;
	s = strtok(NULL, " ");	/* get next string */
      } else if (strcmp(s, "-mbuf") == 0) {
	s = strtok(NULL, " ");
	if (s == NULL) {	/* error */
	  return (UDIErrorInvalidTIPOption);	/* UNKNOWN */
	} else {
	  if (sscanf(s, "%d", &MaxMsgBufSize) != 1)
	    return (UDIErrorInvalidTIPOption);
	};
	s = strtok(NULL, " ");	/* get next string */
      } else if (strcmp(s, "-del") == 0) {
	s = strtok(NULL, " ");
	if (s == NULL) {	/* error */
	  return (UDIErrorInvalidTIPOption);	/* UNKNOWN */
	} else {
	  if (sscanf(s, "%d", &DelayFactor) != 1)
	    return (UDIErrorInvalidTIPOption);
	};
	s = strtok(NULL, " ");	/* get next string */
      } else if (strcmp(s, "-bl") == 0) {
	s = strtok(NULL, " ");
	if (s == NULL) {	/* error */
	  return (UDIErrorInvalidTIPOption);	/* UNKNOWN */
	} else {
	  if (sscanf(s, "%d", &BlockCount) != 1)
	    return (UDIErrorInvalidTIPOption);
	};
	s = strtok(NULL, " ");	/* get next string */
      } else if (strcmp(s, "-to") == 0) {
	s = strtok(NULL, " ");
	if (s == NULL) {	/* error */
	  return (UDIErrorInvalidTIPOption);	/* UNKNOWN */
	} else {
	  if (sscanf(s, "%ld", &TimeOut) != 1)
	    return (UDIErrorInvalidTIPOption);
	};
	s = strtok(NULL, " ");	/* get next string */
      } else if (strcmp(s, "-seg") == 0) {
	s = strtok(NULL, " ");
	if (s == NULL) {	/* error */
	  return (UDIErrorInvalidTIPOption);	/* UNKNOWN */
	} else {
	  if (sscanf(s, "%lx", &tip_config.PC_mem_seg) != 1)
	    return (UDIErrorInvalidTIPOption);
	};
	s = strtok(NULL, " ");	/* get next string */
      } else if (strcmp(s, "-nblock") == 0) { /* specify NBLOCK Stdin Mode */
	PgmStdinMode = TIP_NBLOCK;
	s = strtok(NULL, " ");	/* get next string */
      } else if (strcmp(s, "-port") == 0) {
	s = strtok(NULL, " ");
	if (s == NULL) {	/* error */
	  return (UDIErrorInvalidTIPOption);	/* UNKNOWN */
	} else {
	  if (sscanf(s, "%lx", &tip_config.PC_port_base) != 1)
	    return (UDIErrorInvalidTIPOption);
	};
	s = strtok(NULL, " ");	/* get next string */
      } else if (strcmp(s, "-baud") == 0) {
	s = strtok(NULL, " ");
	if (s == NULL) {	/* error */
	  return (UDIErrorInvalidTIPOption);	/* UNKNOWN */
	} else {
	  (void) strcpy(tip_config.baud_rate, s);
	};
	s = strtok(NULL, " ");	/* get next string */
      } else	/* unknown option */
	return (UDIErrorInvalidTIPOption);	/* UNKNOWN */
    }
  };	/* end while */
  return ((int) 0); /* SUCCESS */
}

static int
write_args(argstring, argstart, datahigh)
  char           *argstring;
  ADDR32          argstart;
  ADDR32         *datahigh;
{
  char           *argvstring[25];
  int             i;
  char           *s;

  i = 0;
  if (argstring == NULL) {
    s = strtok(argstring, " \t\n\r");
    argvstring[i] = s;
  } else {
     (void) strcpy (&TempArgString[0], argstring);
     s = strtok (&TempArgString[0], " \t\n\r");
     argvstring[i] = s;
     if (s != NULL) {
        while ((s = strtok((char *) 0, " \t\n\r"))) {
          i++;
          argvstring[i] = s;
          (void) strcpy(argvstring[i], s);
        };
        /* add the final NULL */ /* i is the argc count */
        argvstring[++i] = NULL;
     }
  }
  return (write_argv(i, argvstring, argstart, datahigh));
}

static int
write_argv(arg_count, arg_ptr, argstart, hi_data)
  int             arg_count;
  char           *arg_ptr[];
ADDR32          argstart;
ADDR32         *hi_data;

{

  int             i;

  UDIError        retval;
  UDIResource     to;
  UDIBool         hostendian;
  UDICount        count,
                  bytes_ret;
  UDISizeT          size;
  UDIUInt32       dataend;	/* start address for heap */
  UDIUInt32       tmp_dataend;	/* start address for heap */

  /*
   * * Write args to target
   */

  /* Set init.data_end to start of arg string space */
  /* (saving room for the array of pointers) */
  dataend = argstart + (arg_count + 1) * sizeof(CPUOffset);

  for (i = 0; i < arg_count; i = i + 1) {

    /* Write arg_ptr[i] pointer (Am29000 address) to target */
    tmp_dataend = dataend;
    /* We might have to change the endian of the address */
    if (tip_target_config.P29KEndian != tip_target_config.TipEndian) {
      tip_convert32((BYTE *) &(tmp_dataend));
    }
    to.Offset = argstart + (i * sizeof(CPUOffset));
    to.Space = UDI29KDRAMSpace;
    count = (UDICount) 1;
    size = (UDISizeT) sizeof(CPUOffset);
    hostendian = FALSE;	/* ???? */
    if ((retval = UDIWrite((UDIHostMemPtr) &tmp_dataend,
			   to,
			   count,
			   size,
			   &bytes_ret,
			   hostendian)) != UDINoError) {
      return (retval);
    };
    /* Continue if SUCCESSful */
    /* Write arg_ptr[i] to target */
    to.Offset = dataend;
    to.Space = UDI29KDRAMSpace;
    count = (UDICount) strlen(arg_ptr[i]) + 1;
    size = (UDISizeT) 1;
    hostendian = FALSE;
    if ((retval = UDIWrite(arg_ptr[i],
			   to,
			   count,
			   size,
			   &bytes_ret,
			   hostendian)) != UDINoError) {
      return (retval);
    };

    dataend = dataend + strlen(arg_ptr[i]) + 1;

  }	/* end for loop */

  /* return dataend */
  *hi_data = dataend;
  /* Write NULL pointer at end of argv array */
  to.Offset = argstart + arg_count * sizeof(CPUOffset);
  to.Space = UDI29KDRAMSpace;
  count = (UDICount) sizeof(CPUOffset);
  size = (UDISizeT) 1;
  hostendian = FALSE;

  if ((retval = UDIWrite("\0\0\0",
			 to,
			 count,
			 size,
			 &bytes_ret,
			 hostendian)) != UDINoError) {
    return (retval);
  };
  return (UDINoError);
}

void
set_stdin_needed(hif_lr3, hif_lr4)
  ADDR32          hif_lr3;
  UDICount        hif_lr4;
{
  Lr3_addr = (CPUOffset) hif_lr3;
  Lr4_count = (UDIUInt32) hif_lr4;
  ProcessorState = (UDIUInt32) UDIStdinNeeded;
  PreviousProcessorState = (UDIUInt32) UDIStdinNeeded;
}

void
set_stdout_ready(hif_lr3, hif_lr4)
  ADDR32          hif_lr3;
  UDICount        hif_lr4;
{
  Lr3_addr = (CPUOffset) hif_lr3;
  Lr4_count = (UDIUInt32) hif_lr4;
  ProcessorState = (UDIUInt32) UDIStdoutReady;
  PreviousProcessorState = (UDIUInt32) UDIStdoutReady;
}

void
set_stderr_ready(hif_lr3, hif_lr4)
  ADDR32          hif_lr3;
  UDICount        hif_lr4;
{
  Lr3_addr = (CPUOffset) hif_lr3;
  Lr4_count = (UDIUInt32) hif_lr4;
  ProcessorState = (UDIUInt32) UDIStderrReady;
  PreviousProcessorState = (UDIUInt32) UDIStderrReady;
}

static  INT32
Wait_For_Ack( /* retries */ )
{
  INT32           code;
  UINT32		  count;

  count=(UINT32) 1;

  code = FAILURE;
  while ((code == FAILURE) && (count < TimeOut)) {
    code = Mini_msg_recv(BLOCK);
    count = count + 10;
    /* Check for user interrupt */
    SIGINT_POLL
    if (StopFlag) {
       STOP_SIG_HDLR
       ProcessorState = (UDIUInt32) UDIStopped;
       PreviousProcessorState = (UDIUInt32) UDIStopped;
       return (ABORT_FAILURE);
    }; 
  };
  return (code);
}

static  INT32
CheckForMsg(time)
  INT32           time;
{
  INT32             i;
  INT32           Code;
  int             ForEver;


  ForEver = 0;
  if (time == (UDIInt32) UDIWaitForever) 
    ForEver = 1;
  else 
   if (RemoteTarget == 1)	/* remote targets */
#ifdef MSDOS
       time = time*100;
#else
       time = time;
#endif

  i = 0;
  while ((i <= time) || ForEver) {
    /* Check for user interrupt */
    SIGINT_POLL
    if (StopFlag) {
       STOP_SIG_HDLR
       ProcessorState = (UDIUInt32) UDIStopped;
       PreviousProcessorState = (UDIUInt32) UDIStopped;
       return (ABORT_FAILURE);
    }; 
    if ((Code = Mini_msg_recv(NONBLOCK)) != FAILURE)
      return (Code);
    i = i + 1;
  }
  return (FAILURE);
}

static  void
process_target_msg(msgcode)
  INT32           msgcode;
{
  switch (msgcode) {
   case HALT:
    if (BreaksInPlace)
      ResetAllBreakpoints();
    BreaksInPlace = 0;
    process_HALT_msg();
    break;
   case CHANNEL1:
    process_CHAN1_msg();
    break;
   case CHANNEL2:
    process_CHAN2_msg();
    break;
   case CHANNEL0_ACK:
    (void) process_chan0_ack();
    break;
   case HIF_CALL:
    (void) process_HIF_msg();
    break;
   case STDIN_NEEDED_REQ:
    (void) process_stdin_needed_req();
    break;
   case STDIN_MODE_REQ:
    (void) set_stdin_mode();
    break;
   case ERROR:
    if (BreaksInPlace)
      ResetAllBreakpoints();
    BreaksInPlace = 0;
    process_ERR_msg();
    break;
   default:
    if (BreaksInPlace)
      ResetAllBreakpoints();
    BreaksInPlace = 0;
    ProcessorState = (UDIUInt32) UDIHalted;
    PreviousProcessorState = (UDIUInt32) UDIHalted;
    fprintf(stderr, "FATAL: a unknown msg 0x%lx\n", msgcode);
    (void)print_recv_bytes();
    break;
  };
}

static INT32
process_chan0_ack()
{
  char	nextchar;

  Channel0Busy = 0;
  if (Channel0_count > 0) {
    Channel0_count = Channel0_count - 1;
    nextchar = channel0_buffer[Channel0_count];
    Mini_build_channel0_msg(&nextchar, (INT32) 1);
    if (Mini_msg_send() != SUCCESS) {
        return((-1) * MONErrCantSendMsg);
    }
#if 0
    Channel0Busy=1;	/* never set */
#endif
  }
  return (UDINoError);
}

static  void
process_HALT_msg()
{
  INT32           mspace;
  ADDR32          pc0,
                  pc1;
  INT32           trap_number;
  INT32		type;
  ADDR32	Inst;
  BreakIdType	break_id;
  INT32		count;


  Mini_unpack_halt_msg(&mspace, &pc0, &pc1, &trap_number);
  if (trap_number == (INT32) 0) {
    if ((break_id = (BreakIdType) is_breakpt_at(mspace, pc1)) 
						> (BreakIdType) 0) {
      ProcessorState = (UDIUInt32) UDIBreak;
      PreviousProcessorState = (UDIUInt32) UDIBreak;
      if ((get_from_bp_table(break_id, &mspace, &pc1, 
				     &count, &type, &Inst) == 0) &&
				     (count < (INT32) 0))
         remove_from_bp_table(break_id);
    } else
      ProcessorState = (UDIUInt32) UDITrapped;
      PreviousProcessorState = (UDIUInt32) UDITrapped;
  } else if (trap_number == 15) {	/* Trace */
    ProcessorState = (UDIUInt32) UDIStepped;
    PreviousProcessorState = (UDIUInt32) UDIStepped;
    StepCmdGiven = 0;	/* step complete */
  } else if (trap_number == 75) {
    ProcessorState = (UDIUInt32) UDIBreak;
    PreviousProcessorState = (UDIUInt32) UDIBreak;
  } else if (trap_number & 0x1000) { /* HIF specific reason */
    if ((trap_number & 0xffff) == 0x1000) { /* HIF exit */
       ProcessorState = (UDIUInt32) (UDIExited | 
					((trap_number & 0xffff0000)>>8));
       PreviousProcessorState = ProcessorState;
    } else { /* HIF error */
       ProcessorState = (UDIUInt32) (UDIHalted | (trap_number & 0xffff0000));
       PreviousProcessorState = ProcessorState;
    }
  } else {
    ProcessorState = (UDIUInt32) (UDITrapped | (trap_number << 8));
    PreviousProcessorState = ProcessorState;
  }
}

static	void
set_stdin_mode()
{
  INT32	mode;

  Mini_unpack_stdin_mode_msg(&mode);
  Mini_build_stdin_mode_ack_msg(PgmStdinMode);
  PgmStdinMode = mode;
  PreviousProcessorState = ProcessorState;	/* save current state */
  ProcessorState = (UDIUInt32) (UDIStdinModeX);
  if (Mini_msg_send() != SUCCESS) {
      ProcessorState = (UDIUInt32) UDIHalted;
  }
  return;
}

static	void
process_stdin_needed_req()
{
  Mini_unpack_stdin_needed_msg(&StdinCharsNeeded);
  /* upper 24 bits gives number needed */
  ProcessorState = (UDIUInt32) (UDIStdinNeeded | (StdinCharsNeeded << 8));
  PreviousProcessorState = ProcessorState;
}

static  void
process_CHAN1_msg()
{
  INT32           count;

  Mini_unpack_channel1_msg((BYTE *) channel1_buffer, &count);
  Channel1_count = (UDISizeT) count;
  /* upper 24 bits gives number to output */
  ProcessorState = (UDIUInt32) (UDIStdoutReady | (Channel1_count << 8));
  PreviousProcessorState = ProcessorState;
}

static  void
process_CHAN2_msg()
{
  INT32           count;

  Mini_unpack_channel2_msg((BYTE *) channel2_buffer, &count);
  Channel2_count = (UDISizeT) count;
  /* upper 24 bits gives number to output */
  ProcessorState = (UDIUInt32) (UDIStderrReady | (Channel2_count << 8));
  PreviousProcessorState = ProcessorState;
}

static  void
process_HIF_msg()
{
  INT32           svc_nm,
                  lr2,
                  lr3,
                  lr4,
                  gr96,
                  gr97,
                  gr121;
  int             retval;
  INT32		MsgCode;

  Mini_unpack_hif_msg(&svc_nm, &lr2, &lr3, &lr4);
  if ((svc_nm == (INT32) HIF_read) && (lr2 == (INT32) 0)) {	/* stdin */
    set_stdin_needed(lr3, lr4);
    return;
  } else if ((svc_nm == (INT32) HIF_write) && (lr2 == (INT32) 2)) {	/* stderr */
    set_stderr_ready(lr3, lr4);
    return;
  } else {
    retval = service_HIF(svc_nm, lr2, lr3, lr4, &gr96, &gr97, &gr121);
    if (retval == (INT32) -1) { /* service failed */
      ProcessorState = (UDIUInt32) UDIHalted;
      PreviousProcessorState = ProcessorState;
      return;
    } ;
    if (svc_nm == HIF_exit) {
	if (BreaksInPlace) {	/* For EB29K */
	  BreaksInPlace = 0;
	  ResetAllBreakpoints();
	};
       if ((tip_target_config.os_version & 0xf) > 4) { /* new HIF kernel */
	  if ((tip_target_config.os_version & 0xf) > 0x6) { /* send hif rtn */
	    Mini_build_hif_rtn_msg(svc_nm, gr121, gr96, gr97);
            if (Mini_msg_send() != SUCCESS) {
	      ProcessorState = (UDIUInt32) UDIHalted;
	      PreviousProcessorState = ProcessorState;
	      return;
	    }
      	    MsgCode = Wait_For_Ack();	/* debug core sends a HALT msg */
      	    if (MsgCode == ABORT_FAILURE) {
	      ProcessorState = (UDIUInt32) UDIHalted;
	      PreviousProcessorState = ProcessorState;
      	    } else if (MsgCode == FAILURE) {
	      ProcessorState = (UDIUInt32) UDIHalted;
	      PreviousProcessorState = ProcessorState;
	    } else {
	      ProcessorState = (UDIUInt32) (UDIExited | (lr2 << 8));
	      PreviousProcessorState = ProcessorState;
	    }
	    return;
	  } else {
	    ProcessorState = (UDIUInt32) (UDIExited | (lr2 << 8));
	    PreviousProcessorState = ProcessorState;
            return;
	  }
       } else { /* old HIF kernel */
	  exitstat = (int) lr2;
          if (Write_Glob_Reg(gr121, (int) 121) != UDINoError) {
	      ProcessorState = (UDIUInt32) UDIHalted;
	      PreviousProcessorState = ProcessorState;
	      return;
          }
	  ProcessorState = (UDIUInt32) (UDIExited | (exitstat << 8));
	  PreviousProcessorState = ProcessorState;
          return;
       }
    } else {	/* not a HIF exit */
       if ((tip_target_config.os_version & 0xf) > 4) { /* new HIF kernel */
	  Mini_build_hif_rtn_msg(svc_nm, gr121, gr96, gr97);
          if (Mini_msg_send() != SUCCESS) {
	    ProcessorState = (UDIUInt32) UDIHalted;
	    PreviousProcessorState = ProcessorState;
	    return;
	  }
	  ProcessorState = (UDIUInt32) UDIRunning;
	  PreviousProcessorState = ProcessorState;
	  return;
       } else { /* old HIF kernel */
          if (Write_Glob_Reg(gr96, (int) 96) != UDINoError) {
	    ProcessorState = (UDIUInt32) UDIHalted;
	    PreviousProcessorState = ProcessorState;
          }
          if (svc_nm == (INT32) HIF_gettz) {
	    if (Write_Glob_Reg(gr97, (int) 97) != UDINoError) {
	      ProcessorState = (UDIUInt32) UDIHalted;
	      PreviousProcessorState = ProcessorState;
	    }
          }
          if (Write_Glob_Reg(gr121, (int) 121) != UDINoError) {
	    ProcessorState = (UDIUInt32) UDIHalted;
	    PreviousProcessorState = ProcessorState;
	  }
          /* UDIExecute()? */
          if (StepCmdGiven) {
	    ProcessorState = UDIStepped;
	    PreviousProcessorState = ProcessorState;
          } else {
	      if (!BreaksInPlace) {
	        PutAllBreakpoints();
	        BreaksInPlace = 1;
	      }
	      UDIExecute();
          }
       };
      return;
    }
  }
}

static  void
process_ERR_msg()
{
  ProcessorState = (UDIUInt32) UDIStopped;
  PreviousProcessorState = ProcessorState;
}

static int
Write_Glob_Reg(RegVal, RegNum)
  INT32           RegVal;
  int             RegNum;
{
  UDIResource     to;
  UDISizeT          size;
  UDICount        mincount;
  UDIBool         hostendian;
  UDICount        bytes_ret;
  UDIError        retval;

  to.Space = (CPUSpace) UDI29KGlobalRegs;
  to.Offset = (CPUOffset) RegNum;
  size = (UDISizeT) 4;
  mincount = (UDICount) 1;
  hostendian = FALSE;
  if (tip_target_config.TipEndian != tip_target_config.P29KEndian) {
    tip_convert32((BYTE *) &RegVal);
  }
  if ((retval = UDIWrite((UDIHostMemPtr) &RegVal,
			 to,
			 mincount,
			 size,
			 &bytes_ret,
			 hostendian)) != UDINoError) {
    return ((UDIError) retval);
  };
  return (UDINoError);
}

static int
PutAllBreakpoints()
{
  UDIResource     addr;
  UDIError        UDIretval;
  UDICount        count_done;
  BreakIdType     break_id;
  ADDR32          offset;
  INT32           space;
  INT32           pcount;
  INT32           type;
  ADDR32          Inst;
  UDIUInt32       IllOp;

  if (strcmp (TargetType, "eb29k") == 0) { /* if EB29K */
    for (break_id = 1; break_id < LastBreakId; break_id++) {
      if (get_from_bp_table(break_id, &space, &offset,
			    &pcount, &type, &Inst) == 0) {
        addr.Offset = offset;
        addr.Space = SpaceMap_mm2udi(space);
	/* Read Instruction into Breaktable */
        if (!strcmp(TargetType, "eb29k")) {	
          if ((UDIretval = UDIRead(addr,
			     (UDIHostMemPtr) &Inst,
			     (UDICount) 1,
			     (UDISizeT) 4,
			     (UDICount *) &count_done,
			     (UDIBool) FALSE)) != UDINoError)	/* 29K endian */
            return (UDIretval);
        };
	(void) update_breakpt_at(space, offset, Inst);
        /* UDIWrite(Illop); write illegal opcode instruction */
        IllOp = (UDIUInt32) 0x00ccbbaa;	/* Illegal opcode */
        if ((UDIretval = UDIWrite((UDIHostMemPtr) &IllOp,
				  addr,
				  (UDICount) 1,
				  (UDISizeT) 4,
				  &count_done,
				  TRUE)) != UDINoError)
	  return (UDIretval);
      }
    }
  }
  return (0);
}

static int
ResetAllBreakpoints()
{
  UDIResource     addr;
  UDIError        UDIretval;
  UDICount        count_done;
  BreakIdType     break_id;
  ADDR32          offset;
  INT32           space;
  INT32           pcount;
  INT32           type;
  ADDR32          Inst;

  if (strcmp (TargetType, "eb29k") == 0) { /* if EB29K */
    for (break_id = 1; break_id < LastBreakId; break_id++) {
      if (get_from_bp_table(break_id, &space, &offset,
			    &pcount, &type, &Inst) == 0) {
        /* UDIWrite(); write original instruction */
        addr.Offset = offset;
        addr.Space = SpaceMap_mm2udi(space);
        if ((UDIretval = UDIWrite((UDIHostMemPtr) &Inst,
				  addr,
				  (UDICount) 1,
				  (UDISizeT) 4,
				  &count_done,
				  FALSE)) != UDINoError)
	  return (UDIretval);
      }
    }
  }
  return (0);
}

void
TIPPrintUsage(s)
  char           *s;
{
  fprintf(stderr, "Minimon (UDI 1.2) TIP Version: %s Date: %s\n", TIPVERSION, TIPDATE);
  fprintf(stderr, "List of Valid MONTIP options are:\n");
  fprintf(stderr, "-t <target_if_type> - the target interface type MUST be specified\n");
  fprintf(stderr, "[-r <Minimon_OS_linked_object>] - ROM file to be downloaded, if any\n");
  fprintf(stderr, "[-m <msg_log_filename>] - file to log messages between TIP and target\n");
  fprintf(stderr, "[-com <serial_comm_port>] -communication port to use, ex: -com com1:\n");
  fprintf(stderr, "[-par <paralle_port>] -parallel port for download , ex: -par lpt1:\n");
  fprintf(stderr, "[-re <retries_for_a_msg>] - number of retries\n");
  fprintf(stderr, "[-le] - specifies target is little endian\n");
  fprintf(stderr, "[-mbuf <msg_bufsize_to_use>] - maximum message buffer size\n");
  fprintf(stderr, "[-bl <msg_block_loopcount>] - block count while receiving\n");
  fprintf(stderr, "[-to <timeout_loopcount>] - timeout while receiving\n");
  fprintf(stderr, "[-seg <PC_mem_seg_addr_in_hex>] - PC memory segment base address, ex: -seg D800\n");
  fprintf(stderr, "[-port <PC_port_base_addr_in_hex>] - PC i/o port base address, ex: -port 2A0\n");
  fprintf(stderr, "[-baud <baudrate>] - baud rate for serial communications, ex: -baud 38400\n");
  fprintf(stderr, "[-R/P] - physical or protected mode (ONLY with OSBOOT supplied)\n");
  fprintf(stderr, "[-S] - run in supervisor mode (ONLY with OSBOOT supplied)\n");
  exit(1);
}

static INT32
SendConfigWait()
{
    INT32	MsgCode;
    int	count;
    unsigned long	time;

    count = (UINT32) 0;
    MsgCode = (INT32) FAILURE;
    do {
#ifdef	MSDOS
        if (RemoteTarget == 1)
           Mini_reset_comm();  /* reset communications channel */
#endif
      Mini_build_config_req_msg();
      if (Mini_msg_send() != SUCCESS) {
        return ((-1) * MONErrCantSendMsg);
      }
      count = count + 1;
      time = (unsigned long) 1;
      do {
	time = time + 10;
        MsgCode = Mini_msg_recv(BLOCK);
        SIGINT_POLL
        if (Interrupted) {
	   Interrupted = 0;
	   return ((INT32) ABORT_FAILURE);
        }
#ifndef	MSDOS
        if ((MsgCode == FAILURE) && 
	    (RemoteTarget == 1))
           Mini_reset_comm();  /* reset communications channel */
#endif
      } while ((MsgCode == FAILURE) && (time < TimeOut));
    } while ((MsgCode == FAILURE) && (count < MessageRetries)); 
    return (MsgCode);
}

static char *
GetTargetType( Target, FromString)
char	*Target;
char	*FromString;
{
  char		*s;

  if (FromString == NULL)
     return (NULL);

  (void) strcpy (&ConnectString[0], FromString); /* to preserve the original */
  s = strtok(ConnectString, " ");
  while (s != NULL) {
    if ((s[0] == '-') && (s[1] == 't') && (s[2] == '\0')) { /* -t option */
       s = strtok (NULL, " "); /* continue search */
       if (s == NULL)
	  return (NULL);
       else {
	  Target = s;
	  return (s);
       }
    };
    s = strtok (NULL, " "); /* continue search */
  }; /* while */
  return (NULL);
}


static INT32
Mini_load_coff(filename, mspace, sym, Section, quietmode)
   char *filename;
   int   quietmode;
   INT32	sym;
   INT32	Section;
   INT32	mspace;
   {
   unsigned short  COFF_sections;
   INT32  flags;
   INT32  memory_space;
   INT32  address;
   INT32  byte_count;
   INT32  write_count;
   INT32  temp_byte_count;

   struct  filehdr      COFF_header;
   struct  aouthdr      COFF_aout_header;
   struct  scnhdr      COFF_section_header;

   /* Open the COFF input file (if we can) */
   if ((coff_in = FindFile(filename)) == NULL)
      return ((-1) * MONErrCantOpenCoff);

   /* Read in COFF header information */
   if (fread((char *)&COFF_header, sizeof(struct filehdr), 1, coff_in) != 1) {
      fclose(coff_in); return ((-1) * MONErrCantLoadROMfile);
   };


   /* Is it an Am29000 COFF File? */
   if ((COFF_header.f_magic != 0x17a) && (COFF_header.f_magic != 0x7a01) &&
       (COFF_header.f_magic != 0x17b) && (COFF_header.f_magic != 0x7b01)) {
      fclose(coff_in); return ((-1) * MONErrCantLoadROMfile);
   }

   /* Get number of COFF sections */
   if ((COFF_header.f_magic != 0x17a) && (COFF_header.f_magic != 0x017b))
      tip_convert16((BYTE *) &COFF_header.f_nscns);
   COFF_sections = (unsigned short) COFF_header.f_nscns;

   /* Read in COFF a.out header information (if we can) */
   if (COFF_header.f_opthdr > 0) {
      if (fread((char *)&COFF_aout_header, sizeof(struct aouthdr), 
						   1, coff_in) != 1) {
         fclose(coff_in); return ((-1) * MONErrCantLoadROMfile);
      };
      if ((COFF_header.f_magic != 0x17a) && (COFF_header.f_magic != 0x017b)) {
         tip_convert16((BYTE *) &COFF_header.f_opthdr);
      }
   }


   /*
   ** Process COFF section headers
   */

   /* Process all sections */
   while ((int) COFF_sections--) {

      fseek (coff_in, (long) (FILHSZ+(int)COFF_header.f_opthdr+
			      SCNHSZ*(COFF_header.f_nscns-COFF_sections-1)), 
			      FROM_BEGINNING);

      if (fread(&COFF_section_header, 1, SCNHSZ, coff_in) != SCNHSZ) {
          fclose(coff_in); return ((-1) * MONErrCantLoadROMfile);
      }

      if ((COFF_header.f_magic != 0x17a) && (COFF_header.f_magic != 0x017b)) {
         tip_convert32((BYTE *) &(COFF_section_header.s_paddr));
         tip_convert32((BYTE *) &(COFF_section_header.s_scnptr));
         tip_convert32((BYTE *) &(COFF_section_header.s_size));
         tip_convert32((BYTE *) &(COFF_section_header.s_flags));
       }

      address = COFF_section_header.s_paddr;
      byte_count = COFF_section_header.s_size;
      flags = COFF_section_header.s_flags;

      /* Print downloading messages (if necessary) */
      if ((flags == (INT32) STYP_TEXT) || (flags == (INT32) (STYP_TEXT | STYP_ABS))) {
	 memory_space = I_MEM;
      } else if ((flags == (INT32) STYP_DATA) || (flags == (INT32) (STYP_DATA | STYP_ABS)) ||
          (flags == (INT32) STYP_LIT) || (flags == (INT32) (STYP_LIT | STYP_ABS)) ||
          (flags == (INT32) STYP_BSS) || (flags == (INT32) (STYP_BSS | STYP_ABS))) {
	 memory_space = D_MEM;
      } else {
	 flags = (INT32) 0;
      }

      if ((flags == (INT32) STYP_BSS) || (flags == (INT32) (STYP_BSS | STYP_ABS))) {
      /* Clear BSS section */
   	if (flags & Section) {
	   (void) memset ((char *) buffer, (int) '\0', sizeof(buffer));
	   while (byte_count > 0) {
	     write_count = (byte_count < (INT32) sizeof(buffer)) ?
				byte_count : (INT32) sizeof (buffer);
	     if(Mini_write_memory ((INT32) memory_space,
			      (ADDR32) address,
			      (INT32) write_count,
			      (BYTE *) buffer) != SUCCESS) {
   		(void) fclose(coff_in);
		return((-1) * MONErrCantWriteToMem);
	     }
	     address = address + write_count;
	     byte_count = byte_count - write_count;
	   }
	}
      } else if (flags & Section) { /* not a BSS or COmment */
	 if (flags == (INT32) (flags & Section)) {
	   fseek (coff_in, COFF_section_header.s_scnptr, FROM_BEGINNING);
           while (byte_count > 0) {
             temp_byte_count = MIN((INT32) byte_count, (INT32) sizeof(buffer));
             if (fread((char *) buffer, (int) temp_byte_count, 1, coff_in) != 1) {
                fclose(coff_in); return ((-1) * MONErrCantLoadROMfile);
	     };
             /* Write to 29K memory*/
             if (Mini_write_memory ((INT32)  memory_space,
                            	(ADDR32) address,
                            	(INT32)  temp_byte_count,
                            	(BYTE *) buffer) != SUCCESS) {
   	       (void) fclose(coff_in);
	       return((-1) * MONErrCantWriteToMem);
	     };
             address = address + temp_byte_count;
             byte_count = byte_count - temp_byte_count;
           };
	 };
      }
   }  /* end while */

   (void) fclose(coff_in);
   return (SUCCESS);

   }   /* end Mini_loadcoff() */

/* 
** Breakpoint code 
*/

static void
add_to_bp_table(id, space, offset, count, type, inst)
BreakIdType	*id;
INT32	space;
ADDR32	offset;
INT32 	count;
INT32	type;
ADDR32	inst;
{
  static BreakIdType	current_break_id=1;
  struct tip_break_table  	*temp, *temp2;

  if (bp_table == NULL) { /* first element */
    bp_table = (struct tip_break_table *) malloc (sizeof(struct tip_break_table));
    bp_table->id = current_break_id;
    bp_table->offset = offset;
    bp_table->space = space;
    bp_table->count = count;
    bp_table->type = type;
    bp_table->BreakInst = inst;
    bp_table->next = NULL;
  } else {
    temp2 = bp_table;
    temp = (struct tip_break_table *) malloc (sizeof(struct tip_break_table));
    temp->id = current_break_id;
    temp->offset = offset;
    temp->space = space;
    temp->count = count;
    temp->type = type;
    temp->BreakInst = inst;
    temp->next = NULL;
    while (temp2->next != NULL)
      temp2 = temp2->next;
    temp2->next = temp;
  };
  *id = current_break_id;
  current_break_id++;
}

static int 
get_from_bp_table(id, space, offset, count, type, inst)
BreakIdType	id;
INT32	*space;
ADDR32	*offset;
INT32 	*count;
INT32	*type;
ADDR32	*inst;
{
  struct tip_break_table  *temp;

  temp = bp_table;

  while (temp != NULL) {
    if (temp->id == id) {
       *offset = temp->offset;
       *space = temp->space;
       *count = temp->count;
       *type = temp->type;
       *inst = temp->BreakInst;
       return(0);
    } else {
      temp = temp->next;
    };
  }
  return(-1);
}

static int
remove_from_bp_table(id)
BreakIdType	id;
{
  struct  tip_break_table	*temp, *temp2;

  if (bp_table == NULL)
     return (-1);
  else {
    temp = bp_table;
    if (temp->id == id) { /* head of list */
       bp_table = bp_table->next;
       (void) free (temp);
       return (0); /* success */
    } else {
       while (temp->next != NULL) {
          if (temp->next->id == id) {
	     temp2 = temp->next;
	     temp->next = temp->next->next;
	     (void) free (temp2);
	     return (0); /* success */
          } else {
            temp = temp->next;
          }
       };
    }
  };
  return (-1);  /* failed */
}

static int 
update_breakpt_at(space, offset, Inst)
INT32	space;
ADDR32	offset;
ADDR32	Inst;
{
  struct tip_break_table  *temp;

  temp = bp_table;

  while (temp != NULL) {
    if ((temp->space == space) && (temp->offset == offset)) {
       temp->BreakInst = Inst;
       return(0);
    } else {
      temp = temp->next;
    };
  }
  return (-1);
}

static int 
is_breakpt_at(space, offset)
INT32	space;
ADDR32	offset;
{
  struct tip_break_table  *temp;

  temp = bp_table;

  while (temp != NULL) {
    if ((temp->space == space) && (temp->offset == offset)) {
       return((int) temp->id); /* TRUE */
    } else {
      temp = temp->next;
    };
  }
  return(0); /* FALSE */
}

#ifdef	MSDOS
#define	PATH_DELIM	";"
#define	DIR_SEP_CHAR	(char) '\\'
#define	APPEND_PATH	"\\lib\\"
#else
#define	PATH_DELIM	":"
#define	DIR_SEP_CHAR		(char) '/'
#define	APPEND_PATH	"/lib/"
#endif
static FILE  *
FindFile(filename)
char	*filename;
{
  char	*path, *pathptr;
  char	*trypath, *at;
  char	*pathbuf;
  FILE	*filep;

  /* is the filename given already complete? */
  if ((filep = fopen(filename, FILE_OPEN_FLAG)) != NULL) {
      return(filep);
  };

  /* get PATH */
  if ((pathptr = (char *) getenv ("PATH")) == NULL)
     return ((FILE *) NULL);
  if ((path = (char *) malloc ((unsigned int)strlen(pathptr)+1))==NULL)
     return ((FILE *) NULL);
  (void) strcpy(path,pathptr); /* local copy */
  /* alloc buffer */
  if ((pathbuf = (char *) malloc((unsigned int) strlen(path)+
		   strlen("../lib/ ")+strlen(filename)+3)) == NULL)
     return ((FILE *) NULL);

  /* get first item */
  if ((trypath = strtok(path, PATH_DELIM)) == NULL) { /* one item */
     (void) strcpy(pathbuf,path);
     if ((at = strrchr (pathbuf, DIR_SEP_CHAR)) != NULL) {
         (void) strcpy (at, APPEND_PATH);
	 (void) strcat (pathbuf, filename);
         if ((filep = fopen(pathbuf, FILE_OPEN_FLAG)) != NULL) 
	   return (filep);
     } else { /* just append filename */
         (void) strcat (pathbuf, APPEND_PATH);
	 (void) strcat (pathbuf, filename);
         if ((filep = fopen(pathbuf, FILE_OPEN_FLAG)) != NULL) 
	   return (filep);
     };
     return ((FILE *) NULL);
  };

  /* try all items */
  while (trypath != NULL) {
      (void) strcpy (pathbuf, trypath);
      if ((at = strrchr (pathbuf, DIR_SEP_CHAR)) != NULL) {
         (void) strcpy (at, APPEND_PATH);
	 (void) strcat (pathbuf, filename);
         if ((filep = fopen(pathbuf, FILE_OPEN_FLAG)) != NULL) 
	   return (filep);
      } else { /* just append filename */
         (void) strcat (pathbuf, APPEND_PATH);
	 (void) strcat (pathbuf, filename);
         if ((filep = fopen(pathbuf, FILE_OPEN_FLAG)) != NULL) 
	   return (filep);
      };
      trypath = strtok((char *) 0, PATH_DELIM);
  }

  /* didn't succeed */
  return ((FILE *) NULL);
}

void
IntHandler(num)
int	num;
{
  Interrupted = 1;
  return;
}
