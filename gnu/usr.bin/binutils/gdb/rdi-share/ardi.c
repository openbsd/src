/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/*
 * ARDI.c
 * Angel Remote Debug Interface
 *
 *
 * $Revision: 1.3 $
 *     $Date: 2004/12/27 14:00:53 $
 *
 * This file is based on /plg/pisd/rdi.c, but instead of using RDP it uses
 * ADP messages.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define uint HIDE_HPs_uint
#include <signal.h>
#undef uint


#include "angel_endian.h"
#include "ardi.h"
#include "buffers.h"
#include "channels.h"
#include "hostchan.h"
#include "host.h"
#include "angel_bytesex.h"
#include "dbg_cp.h"
#include "adp.h"
#include "hsys.h"
#include "logging.h"
#include "msgbuild.h"
#include "rxtx.h"
#include "devsw.h"
#include "params.h"

#ifdef COMPILING_ON_WINDOWS
#  define IGNORE(x) (x = x)   /* must go after #includes to work on Windows */
#endif
#define NOT(x) (!(x))

#define ADP_INITIAL_TIMEOUT_PERIOD 5

static volatile int executing;
static int rdi_log = 0 ; /* debugging  ? */

/* we need a starting point for our first buffers, this is a safe one */
int Armsd_BufferSize = ADP_BUFFER_MIN_SIZE;
int Armsd_LongBufSize = ADP_BUFFER_MIN_SIZE;

#ifdef WIN32
  extern int interrupted;
  extern int swiprocessing;
#endif

static char dummycline = 0;
char *ardi_commandline = &dummycline ; /* exported in ardi.h */

extern unsigned int heartbeat_enabled;

static unsigned char *cpwords[16];

typedef struct stoppedProcListElement {
  struct stoppedProcListElement *next;
  angel_RDI_TargetStoppedProc *fn;
  void *arg;
} stoppedProcListElement;

static stoppedProcListElement *stopped_proc_list=NULL;

const struct Dbg_HostosInterface *angel_hostif;
static hsys_state *hstate;

static void angel_DebugPrint(const char *format, ...)
{ va_list ap;
  va_start(ap, format);
  angel_hostif->dbgprint(angel_hostif->dbgarg, format, ap);
  va_end(ap);
}

#ifdef RDI_VERBOSE
#define TracePrint(s) \
  if (rdi_log & 2) angel_DebugPrint("\n"); \
  if (rdi_log & 1) angel_DebugPrint s
#else
#define TracePrint(s)
#endif

typedef struct receive_dbgmsg_state {
  volatile int received;
  Packet *packet;
} receive_dbgmsg_state;

static receive_dbgmsg_state dbgmsg_state;

static void receive_debug_packet(Packet *packet, void *stateptr)
{
  receive_dbgmsg_state *state = stateptr;

  state->packet = packet;
  state->received = 1;
}

static int register_debug_message_handler(void)
{
  int err;
  dbgmsg_state.received = 0;

  err = Adp_ChannelRegisterRead(CI_HADP, receive_debug_packet, &dbgmsg_state);
#ifdef DEBUG
  if (err!=adp_ok) angel_DebugPrint("register_debug_message_handler failed %i\n", err);
#endif
  return err;
}


static int wait_for_debug_message(int *rcode, int *debugID,
                                  int *OSinfo1, int *OSinfo2,
                                  int *status, Packet **packet)
{
  unsigned int reason;

#ifdef DEBUG
  angel_DebugPrint("wait_for_debug_message waiting for %X\n", *rcode);
#endif

  for ( ; dbgmsg_state.received == 0 ; )
    Adp_AsynchronousProcessing(async_block_on_read);

#ifdef DEBUG
  angel_DebugPrint("wait_for_debug_message got packet\n");
#endif

  *packet = dbgmsg_state.packet;

  Adp_ChannelRegisterRead(CI_HADP, NULL, NULL);

  /*
   * TODO:
   * If ADP_Unrecognised return error.
   * If ADP_Acknowledge - handle appropriately.
   * If expected message read arguments and return RDIError_NoError.
   * Note: if RDIError occurs then the data values returned are junk
   */

  unpack_message(BUFFERDATA((*packet)->pk_buffer), "%w%w%w%w%w", &reason, debugID,
                 OSinfo1, OSinfo2, status);
  if ((reason&0xffffff) == ADP_HADPUnrecognised)
    return RDIError_UnimplementedMessage;
  if (reason != (unsigned ) *rcode) {
    if((reason&0xffffff) == ADP_HADPUnrecognised)
      return RDIError_UnimplementedMessage;
    else {
      angel_DebugPrint("ARDI ERROR: Expected reasoncode %x got reasoncode %x.\n",
             *rcode, reason);
      return RDIError_Error;
    }
  }
  else
    return RDIError_NoError;
  return RDIError_Error;    /* stop a pesky ANSI compiler warning */
}


/*
 * Handler and registration for logging messages from target
 */
static void TargetLogCallback( Packet *packet, void *state )
{
    p_Buffer     reply = BUFFERDATA(packet->pk_buffer);
    unsigned int len   = packet->pk_length;
    IGNORE(state);
    angel_hostif->write(angel_hostif->hostosarg,
                        (char *)reply, len - CHAN_HEADER_SIZE);
    DevSW_FreePacket(packet);

    packet = DevSW_AllocatePacket(4); /* better not ask for 0 */
    /* the reply is the ACK - any contents are ignored */
    if (packet != NULL)
       Adp_ChannelWrite( CI_TLOG, packet );
}

static void TargetLogInit( void )
{
    AdpErrs err = Adp_ChannelRegisterRead( CI_TLOG, TargetLogCallback, NULL );

#ifdef DEBUG
    if (err != adp_ok)
       angel_DebugPrint("CI_TLOG RegisterRead failed %d\n", err);
#else
    IGNORE(err);
#endif
}

/*----------------------------------------------------------------------*/
/*----angel_RDI_open-----------------------------------------------------*/
/*----------------------------------------------------------------------*/

typedef struct NegotiateState {
      bool             negotiate_resp;
      bool             negotiate_ack;
      bool             link_check_resp;
      ParameterConfig *accepted_config;
} NegotiateState;

static void receive_negotiate(Packet *packet, void *stateptr)
{
    unsigned reason, debugID, OSinfo1, OSinfo2, status;
    NegotiateState *n_state = (NegotiateState *)stateptr;
    p_Buffer reply = BUFFERDATA(packet->pk_buffer);

    unpack_message( reply, "%w%w%w%w",
                    &reason, &debugID, &OSinfo1, &OSinfo2 );
    reply += ADP_DEFAULT_HEADER_SIZE;

#ifdef DEBUG
    angel_DebugPrint( "receive_negotiate: reason %x\n", reason );
#endif

    switch ( reason )
    {
        case ADP_ParamNegotiate | TtoH:
        {
            n_state->negotiate_resp = TRUE;

            status = GET32LE( reply );
            reply += sizeof(word);
#ifdef DEBUG
            angel_DebugPrint( "ParamNegotiate status %u\n", status );
#endif
            if ( status == RDIError_NoError )
            {
                if ( Angel_ReadParamConfigMessage(
                         reply, n_state->accepted_config ) )
                   n_state->negotiate_ack = TRUE;
            }
            break;
        }

        case ADP_LinkCheck | TtoH:
        {
#ifdef DEBUG
            angel_DebugPrint( "PONG!\n" );
#endif
            n_state->link_check_resp = TRUE;
            break;
        }

        default:
        {
#ifdef DEBUG
            angel_DebugPrint( "Unexpected!\n" );
#endif
            break;
        }
    }
    DevSW_FreePacket( packet );
}

# include <sys/types.h>
#ifdef __unix
# include <sys/time.h>
#else
# include <time.h>
#endif

/*
 * convert a config into a single-valued options list
 */
static ParameterOptions *config_to_options( const ParameterConfig *config )
{
    unsigned int        num_params;
    size_t              size;
    ParameterOptions   *base_p;

    num_params  = config->num_parameters;
    size        =
        sizeof(ParameterOptions)
        + num_params*(sizeof(ParameterList) + sizeof(unsigned int));
    base_p      = malloc( size );

    if ( base_p != NULL )
    {
        unsigned int    u;
        ParameterList  *list_p          =
            (ParameterList *)((char *)base_p + sizeof(ParameterOptions));
        unsigned int   *option_p        =
            (unsigned int *)(list_p + num_params);

        base_p->num_param_lists = num_params;
        base_p->param_list = list_p;

        for ( u = 0; u < num_params; ++u )
        {
            option_p[u]                 = config->param[u].value;
            list_p[u].type              = config->param[u].type;
            list_p[u].num_options       = 1;
            list_p[u].option            = &option_p[u];
        }
    }

    return base_p;
}

static AdpErrs negotiate_params( const ParameterOptions *user_options )
{
    Packet                    *packet;
    unsigned int               count;
    static Parameter           params[AP_NUM_PARAMS];
    static ParameterConfig     accepted_config = { AP_NUM_PARAMS, params };

    time_t t;

    static volatile NegotiateState    n_state;
    n_state.negotiate_resp = FALSE;
    n_state.negotiate_ack = FALSE;
    n_state.link_check_resp = FALSE;
    n_state.accepted_config = &accepted_config;
    
#ifdef DEBUG
    angel_DebugPrint( "negotiate_params\n" );
#endif

    Adp_ChannelRegisterRead( CI_HBOOT, receive_negotiate, (void *)&n_state );

    packet = (Packet *)DevSW_AllocatePacket(Armsd_BufferSize);
    count = msgbuild( BUFFERDATA(packet->pk_buffer), "%w%w%w%w",
                      ADP_ParamNegotiate | HtoT, 0,
                      ADP_HandleUnknown, ADP_HandleUnknown );
    count += Angel_BuildParamOptionsMessage(
        BUFFERDATA(packet->pk_buffer) + count, user_options );
    packet->pk_length = count;
    Adp_ChannelWriteAsync( CI_HBOOT, packet );

#ifdef DEBUG
    angel_DebugPrint( "sent negotiate packet\n" );
#endif

    t=time(NULL);

    do {
      Adp_AsynchronousProcessing(async_block_on_nothing);

      if ((time(NULL)-t) > ADP_INITIAL_TIMEOUT_PERIOD) {
        return adp_timeout_on_open;
      }
    } while ( ! n_state.negotiate_resp );

    if ( n_state.negotiate_ack )
    {
        /* select accepted config */
        Adp_Ioctl( DC_SET_PARAMS, (void *)n_state.accepted_config );

        /*
         * 960430 KWelton
         *
         * There is a race in the renegotiation protocol: the
         * target has to have had time to load new config before
         * we send the link check packet - insert a deliberate
         * pause (100ms) to give the target some time
         */
        Adp_delay(100000);

        /* do link check */
        msgsend( CI_HBOOT, "%w%w%w%w", ADP_LinkCheck | HtoT, 0,
                 ADP_HandleUnknown, ADP_HandleUnknown );
#ifdef DEBUG
        angel_DebugPrint("sent link check\n");
#endif

        do {
            Adp_AsynchronousProcessing(async_block_on_read);
        } while ( ! n_state.link_check_resp );
        Adp_initSeq();
    }
    return adp_ok;
}

static int late_booted = FALSE;
static bool ardi_handler_installed = FALSE;

#ifdef __unix
static struct sigaction old_action;
#else
static void (*old_handler)();
#endif

static bool boot_interrupted = FALSE;
static volatile bool interrupt_request = FALSE;
static volatile bool stop_request = FALSE;

static void ardi_sigint_handler(int sig) {
#ifdef DEBUG
    if (sig != SIGINT)
       angel_DebugPrint("Expecting SIGINT got %d.\n", sig);
#else
    IGNORE(sig);
#endif
    boot_interrupted = TRUE;
    interrupt_request = TRUE;
#ifndef __unix
    signal(SIGINT, ardi_sigint_handler);
#endif
}

static void install_ardi_handler( void ) {
  if (!ardi_handler_installed) {
    /* install a new Ctrl-C handler so we can abandon waiting */
#ifdef __unix
    struct sigaction new_action;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_handler = ardi_sigint_handler;
    new_action.sa_flags = 0;
    sigaction(SIGINT, &new_action, &old_action);
#else
    old_handler = signal(SIGINT, ardi_sigint_handler);
#endif
    ardi_handler_installed = TRUE;
  }
}

static int angel_RDI_errmess(char *buf, int blen, int errnum);

static void receive_reset_acknowledge(Packet *packet, void *stateptr) {
  unsigned reason, debugID, OSinfo1, OSinfo2, status;
  IGNORE(stateptr);

  unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w", &reason, &debugID,
                 &OSinfo1, &OSinfo2, &status);
  if (reason==(ADP_Reset | TtoH) && status==AB_NORMAL_ACK) {
#ifdef DEBUG
    angel_DebugPrint("DEBUG: Successfully received normal reset acknowledgement\n");
    late_booted = FALSE;
#endif
  } else if (reason==(ADP_Reset | TtoH) && status==AB_LATE_ACK) {
    char late_msg[AdpMessLen_LateStartup];
    int  late_len;
#ifdef DEBUG
    angel_DebugPrint("DEBUG: Successfully received LATE reset acknowledgement\n");
#endif
    late_booted = TRUE;
    install_ardi_handler();
    late_len = angel_RDI_errmess(late_msg,
                                 AdpMessLen_LateStartup, adp_late_startup);
    angel_hostif->write(angel_hostif->hostosarg, late_msg, late_len);
  } else {
#ifdef DEBUG
    angel_DebugPrint("DEBUG: Bad reset ack: reason=%8X, status=%8X\n", reason, status);
#endif
  }
  DevSW_FreePacket(packet);
}

static int booted_not_received;
static unsigned int angel_version;
static unsigned int adp_version;
static unsigned int arch_info;
static unsigned int cpu_info;
static unsigned int hw_status;

static void receive_booted(Packet *packet, void *stateptr) {
  unsigned reason, debugID, OSinfo1, OSinfo2, banner_length, bufsiz, longsiz;
  unsigned i, count;

  IGNORE(stateptr);

  count = unpack_message(BUFFERDATA(packet->pk_buffer),
                         "%w%w%w%w%w%w%w%w%w%w%w%w",
                 &reason, &debugID, &OSinfo1, &OSinfo2, &bufsiz, &longsiz,
                 &angel_version, &adp_version,
                 &arch_info, &cpu_info, &hw_status, &banner_length);
  if (reason==(ADP_Booted | TtoH)) {
#ifdef MONITOR_DOWNLOAD_PACKETS
    angel_DebugPrint("DEBUG: Successfully received Booted\n");
    angel_DebugPrint("       cpu_info=%8X, hw_status=%8X, bufsiz=%d, longsiz=%d\n",
           cpu_info, hw_status, bufsiz, longsiz);
#endif
    /* Get the banner from the booted message */
    for (i=0; i<banner_length; i++)
      angel_hostif->writec(angel_hostif->hostosarg,
                          (BUFFERDATA(packet->pk_buffer)+count)[i]);

    booted_not_received=0;
#ifndef NO_HEARTBEAT
    heartbeat_enabled = TRUE;
#endif
    Armsd_BufferSize = bufsiz + CHAN_HEADER_SIZE;
    Armsd_LongBufSize = longsiz + CHAN_HEADER_SIZE;
  } else {
#ifdef DEBUG
    angel_DebugPrint("DEBUG: Bad Booted msg: reason=%8X\n", reason);
#endif
  }
  DevSW_FreePacket(packet);
}


/* forward declaration */
static int angel_negotiate_defaults( void );

/* Open communications. */
int angel_RDI_open(
    unsigned type, Dbg_ConfigBlock const *config,
    Dbg_HostosInterface const *hostif, struct Dbg_MCState *dbg_state)
{
  Packet *packet;
  int status, reasoncode, debugID, OSinfo1, OSinfo2, err;
  ParameterOptions *user_options = NULL;

  time_t t;

  IGNORE( dbg_state );

  if ((type & 1) == 0) {
    /* cold start */
    if (hostif != NULL) {
      angel_hostif = hostif;
      err = HostSysInit(hostif, &ardi_commandline, &hstate);
      if (err != RDIError_NoError) {
#ifdef DEBUG
        angel_DebugPrint("DEBUG: HostSysInit error %i\n",err);
#endif
        return err;
      }
    }
    TargetLogInit();
  }

#ifdef DEBUG
  angel_DebugPrint("DEBUG: Buffer allocated in angel_RDI_open(type=%i).\n",type);
#endif

  if ((type & 1) == 0) {
    /* cold start */
    unsigned endian;
    Adp_Ioctl( DC_GET_USER_PARAMS, (void *)&user_options );
    if ( user_options != NULL ) {
      err = negotiate_params( user_options );
      if (err != adp_ok) return err;
    }
    else {
      ParameterConfig *default_config = NULL;
      Adp_Ioctl( DC_GET_DEFAULT_PARAMS, (void *)&default_config );
      if ( default_config != NULL ) {
        ParameterOptions *default_options = config_to_options(default_config);
        err = negotiate_params( default_options );
        if (err != adp_ok) return err;
      }
    }

    /* Register handlers before sending any messages */
    booted_not_received=1;
    Adp_ChannelRegisterRead(CI_HBOOT, receive_reset_acknowledge, NULL);
    Adp_ChannelRegisterRead(CI_TBOOT, receive_booted, NULL);
    endian = 0;
    if (config!=NULL) {
      if (config->bytesex & RDISex_Little) endian |= ADP_BootHostFeature_LittleEnd;
      if (config->bytesex & RDISex_Big) endian |= ADP_BootHostFeature_BigEnd;
    }
    msgsend(CI_HBOOT,"%w%w%w%w%w", ADP_Reset | HtoT, 0,
            ADP_HandleUnknown, ADP_HandleUnknown, endian);
#ifdef DEBUG
    angel_DebugPrint("DEBUG: Transmitted Reset message in angel_RDI_open.\n");
#endif

    /* We will now either get an acknowledgement for the Reset message
     * or if the target was started after the host, we will get a
     * rebooted message first.
     */

#ifdef DEBUG
    angel_DebugPrint("DEBUG: waiting for a booted message\n");
#endif

    {
      boot_interrupted = FALSE;

      if (late_booted)
        install_ardi_handler();

      t=time(NULL);

      do {
        Adp_AsynchronousProcessing(async_block_on_nothing);
        if ((time(NULL)-t) > ADP_INITIAL_TIMEOUT_PERIOD && !late_booted) {
          return adp_timeout_on_open;
        }
      } while (booted_not_received && !boot_interrupted);

      if (ardi_handler_installed)
      {
        /* uninstall our Ctrl-C handler */
#ifdef __unix
        sigaction(SIGINT, &old_action, NULL);
#else
        signal(SIGINT, old_handler);
#endif
      }

      if (boot_interrupted) {
        angel_negotiate_defaults();
        return adp_abandon_boot_wait;
      }
    }

    booted_not_received=1;
    Adp_ChannelRegisterRead(CI_HBOOT, NULL, NULL);

    /* Leave the booted handler installed */
    msgsend(CI_TBOOT, "%w%w%w%w%w", ADP_Booted | HtoT, 0,
            ADP_HandleUnknown, ADP_HandleUnknown, 0);
    Adp_initSeq();
#ifdef DEBUG
    angel_DebugPrint("DEBUG: Transmitted ADP_Booted acknowledgement.\n");
    angel_DebugPrint("DEBUG: Boot sequence completed, leaving angel_RDI_open.\n");
#endif

    return (hw_status & ADP_CPU_BigEndian )? RDIError_BigEndian :
      RDIError_LittleEndian;
  }
  else {
    /* warm start */
    register_debug_message_handler();

    msgsend(CI_HADP, "%w%w%w%w",
            ADP_InitialiseApplication | HtoT, 0,
            ADP_HandleUnknown, ADP_HandleUnknown);
#ifdef DEBUG
    angel_DebugPrint("DEBUG: Transmitted Initialise Application\n");
#endif
    reasoncode=ADP_InitialiseApplication | TtoH;
    err = wait_for_debug_message(&reasoncode, &debugID, &OSinfo1, &OSinfo2,
                                &status, &packet);
    if (err != RDIError_NoError) return err;
    return status;
  }
  return -1;
}


/*----------------------------------------------------------------------*/
/*----angel_RDI_close----------------------------------------------------*/
/*----------------------------------------------------------------------*/

static int angel_negotiate_defaults( void ) {
    int err = adp_ok;
    ParameterConfig *default_config = NULL;
    Adp_Ioctl( DC_GET_DEFAULT_PARAMS, (void *)&default_config );
    if ( default_config != NULL ) {
        ParameterOptions *default_options = config_to_options(default_config);
        err = negotiate_params( default_options );
        free( default_options );
    }
    return err;
}

int angel_RDI_close(void) {
/*Angel host exit */
  int err;
  int status,debugID, OSinfo1,OSinfo2;
  int reason;
  Packet *packet = NULL;;
#ifdef DEBUG
  angel_DebugPrint("DEBUG: Entered angel_RDI_Close.\n");
#endif

  register_debug_message_handler();

  heartbeat_enabled = FALSE;

  err = msgsend(CI_HADP,"%w%w%w%w",ADP_End | HtoT,0,
          ADP_HandleUnknown, ADP_HandleUnknown);
  if (err != RDIError_NoError) return err;
  reason = ADP_End | TtoH;
  err =  wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                                  &status, &packet);
  DevSW_FreePacket(packet);
  if (err != RDIError_NoError) return err;
  if (status == RDIError_NoError) {
    err = angel_negotiate_defaults();
    if (err != adp_ok) return err;
    Adp_Ioctl( DC_RESET, NULL ); /* just to be safe */
    return HostSysExit(hstate);
  }
  else
      return status;
}


/*----------------------------------------------------------------------*/
/*----angel_RDI_read-----------------------------------------------------*/
/*----------------------------------------------------------------------*/

/* Read memory contents from target to host: use ADP_Read */
int angel_RDI_read(ARMword source, void *dest, unsigned *nbytes)
{
  Packet *packet=NULL;
  int len;                               /* Integer to hold message length. */
  unsigned int nbtogo = *nbytes, nbinpacket, nbdone=0;
  int rnbytes = 0, status, reason, debugID, OSinfo1, OSinfo2, err;
  unsigned int maxlen = Armsd_BufferSize-CHAN_HEADER_SIZE-ADP_ReadHeaderSize;

  /* Print debug trace information, this is just copied straight from rdi.c
     and I can see no reason why it should have to be changed. */
  TracePrint(("angel_RDI_read: source=%.8lx dest=%p nbytes=%.8x\n",
                (unsigned long)source, dest, *nbytes));
  if (*nbytes == 0) return RDIError_NoError;       /* Read nothing - easy! */
  /* check the buffer size */
  while (nbtogo >0) {
    register_debug_message_handler();

    nbinpacket = (nbtogo <= maxlen) ? nbtogo : maxlen;
    len = msgsend(CI_HADP, "%w%w%w%w%w%w", ADP_Read | HtoT, 0,
                  ADP_HandleUnknown, ADP_HandleUnknown, source+nbdone,
                  nbinpacket);
    reason=ADP_Read | TtoH;
    err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                                &status, &packet);
    TracePrint(("angel_RDI_read: nbinpacket =%d status=%08x err = %d\n",
                nbinpacket,status,err));
    if (err != RDIError_NoError) return err;       /* Was there an error? */
    if (status == RDIError_NoError){
      rnbytes += PREAD(LE,(unsigned int *)(BUFFERDATA(packet->pk_buffer)+20));
      TracePrint(("angel_RDI_read: rnbytes = %d\n",rnbytes));
      memcpy(((unsigned char *)dest)+nbdone, BUFFERDATA(packet->pk_buffer)+24,
             nbinpacket);
    }
    nbdone += nbinpacket;
    nbtogo -= nbinpacket;
  }
  *nbytes -= rnbytes;
  return status;
}


/*----------------------------------------------------------------------*/
/*----angel_RDI_write----------------------------------------------------*/
/*----------------------------------------------------------------------*/

/* Transfer memory block from host to target.  Use ADP_Write>. */
int angel_RDI_write(const void *source, ARMword dest, unsigned *nbytes)
{
  Packet *packet;/* Message buffers. */
  unsigned int len, nbtogo = *nbytes, nboffset = 0, nbinpacket;
  int status, reason, debugID, OSinfo1, OSinfo2, err;
  unsigned int maxlen = Armsd_LongBufSize-CHAN_HEADER_SIZE-ADP_WriteHeaderSize;

  TracePrint(("angel_RDI_write: source=%p dest=%.8lx nbytes=%.8x\n",
                 source, (unsigned long)dest, *nbytes));

  if (*nbytes == 0) return RDIError_NoError;

  *nbytes = 0;
  while (nbtogo > 0) {
    packet = (Packet *) DevSW_AllocatePacket(Armsd_LongBufSize);
    nbinpacket = (nbtogo <= maxlen) ? nbtogo : maxlen;
    len = msgbuild(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w",
                   ADP_Write | HtoT, 0, ADP_HandleUnknown,
                   ADP_HandleUnknown, dest+nboffset, nbinpacket);
    /* Copy the data into the packet. */

    memcpy(BUFFERDATA(packet->pk_buffer)+len,
           ((const unsigned char *) source)+nboffset, nbinpacket);
    nboffset += nbinpacket;
    packet->pk_length = nbinpacket+len;

#ifdef MONITOR_DOWNLOAD_PACKETS
    angel_DebugPrint("angel_RDI_write packet size=%i, bytes done=%i\n",
            nbinpacket, nboffset);
#endif

    register_debug_message_handler();
    Adp_ChannelWrite(CI_HADP, packet);
    reason=ADP_Write | TtoH;
    err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                                &status, &packet);
    nbtogo -= nbinpacket;
    if (err != RDIError_NoError) return err;
    if (status == RDIError_NoError)
      *nbytes += nbinpacket;

    DevSW_FreePacket(packet);
  }
  return status;
}


/*----------------------------------------------------------------------*/
/*----angel_RDI_CPUread--------------------------------------------------*/
/*----------------------------------------------------------------------*/

/* Reads the values of registers in the CPU, uses ADP_CPUwrite. */
int angel_RDI_CPUread(unsigned mode, unsigned long mask, ARMword *buffer)
{
  unsigned int i, j;
  Packet *packet = NULL;
  int err, status, reason, debugID, OSinfo1, OSinfo2;
#ifdef DEBUG
  angel_DebugPrint("DEBUG: Entered angel_RDI_CPUread.\n");
#endif
  for (i=0, j=0 ; i < RDINumCPURegs ; i++)
    if (mask & (1L << i)) j++;            /* Count the number of registers. */

  register_debug_message_handler();
  msgsend(CI_HADP, "%w%w%w%w%c%w", ADP_CPUread | HtoT, 0,
          ADP_HandleUnknown, ADP_HandleUnknown, mode, mask);
  reason = ADP_CPUread | TtoH;
  err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
  if (err != RDIError_NoError) {
    DevSW_FreePacket(packet);
    return err;
  }
  if(status == RDIError_NoError) {
    for (i=0; i<j; i++)
      buffer[i] = GET32LE(BUFFERDATA(packet->pk_buffer)+20+(i*4));
    TracePrint(("angel_RDI_CPUread: mode=%.8x mask=%.8lx", mode, mask));
    DevSW_FreePacket(packet);
#ifdef RDI_VERBOSE
    if (rdi_log & 1) {
      unsigned k;
      for (k = 0, j = 0 ; j <= 20 ; j++)
        if (mask & (1L << j)) {
          angel_DebugPrint("%c%.8lx",k%4==0?'\n':' ',
                           (unsigned long)buffer[k]);
          k++ ;
        }
      angel_DebugPrint("\n") ;
    }
#endif

  }
  return status;
}

/*----------------------------------------------------------------------*/
/*----angel_RDI_CPUwrite-------------------------------------------------*/
/*----------------------------------------------------------------------*/

/* Write CPU registers: use ADP_CPUwrite. */
int angel_RDI_CPUwrite(unsigned mode, unsigned long mask,
                      ARMword const *buffer){

  unsigned i, j, c;
  Packet *packet;
  int status, reason, debugID, OSinfo1, OSinfo2, err, len;

  TracePrint(("angel_RDI_CPUwrite: mode=%.8x mask=%.8lx", mode, mask));
#ifdef RDI_VERBOSE
 if (rdi_log & 1) {
    for (j = 0, i = 0 ; i <= 20 ; i++)
       if (mask & (1L << i)) {
          angel_DebugPrint("%c%.8lx",j%4==0?'\n':' ',
                           (unsigned long)buffer[j]);
          j++ ;
          }
    angel_DebugPrint("\n") ;
    }
#endif
 packet = (Packet *)DevSW_AllocatePacket(Armsd_BufferSize);
 for (i=0, j=0; i < RDINumCPURegs ; i++)
   if (mask & (1L << i)) j++; /* count the number of registers */

 len = msgbuild(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%b%w",
                ADP_CPUwrite | HtoT, 0,
                ADP_HandleUnknown, ADP_HandleUnknown, mode, mask);
 for(c=0; c<j; c++)
   PUT32LE(BUFFERDATA(packet->pk_buffer)+len+(c*4), buffer[c]);
 packet->pk_length = len+(j*4);
 register_debug_message_handler();

 Adp_ChannelWrite(CI_HADP, packet);
 reason = ADP_CPUwrite | TtoH;
 err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                             &status, &packet);
 unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w", &reason, &debugID,
                &OSinfo1, &OSinfo2, &status);
 DevSW_FreePacket(packet);
 if (err != RDIError_NoError)
   return err;      /* Was there an error? */
 else
   return status;
 }


/*----------------------------------------------------------------------*/
/*----angel_RDI_CPread---------------------------------------------------*/
/*----------------------------------------------------------------------*/

/* Read coprocessor's internal state.  See dbg_cp.h for help.
 * Use ADP_CPRead.
 * It would appear that the correct behaviour at this point is to leave
 * the unpacking to a the caller and to simply copy the stream of data
 * words into the buffer
 */

int angel_RDI_CPread(unsigned CPnum, unsigned long mask, ARMword *buffer){
  Packet *packet = NULL;
  int i, j, status, reasoncode, OSinfo1, OSinfo2, err, debugID;
  unsigned char *rmap = cpwords[CPnum];
  int n;
#ifdef DEBUG
  angel_DebugPrint("DEBUG: Entered angel_RDI_CPread.\n");
#endif
  if (rmap == NULL) return RDIError_UnknownCoPro;

  register_debug_message_handler();
  n = rmap[-1];
  msgsend(CI_HADP, "%w%w%w%w%b%w", ADP_CPread | HtoT, 0,
          ADP_HandleUnknown, ADP_HandleUnknown, CPnum, mask);
  reasoncode=ADP_CPread | TtoH;
  err = wait_for_debug_message(&reasoncode, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
  if (err != RDIError_NoError) {
    DevSW_FreePacket(packet);
    return err;          /* Was there an error? */
  }
  for (j=i=0; i < n ; i++) /* count the number of registers */
    if (mask & (1L << i)) {
      j++;
    }
  for (i=0; i<j; i++)
    buffer[i] = PREAD32(LE, BUFFERDATA(packet->pk_buffer) + 20 + (i*4));
  DevSW_FreePacket(packet);
  TracePrint(("angel_RDI_CPread: CPnum=%.8x mask=%.8lx\n", CPnum, mask));
#ifdef RDI_VERBOSE
  if (rdi_log & 1) {
    for (i = 0, j = 0; j < n ; j++) {
      if (mask & (1L << j)) {
        int nw = rmap[j];
        angel_DebugPrint("%2d ", j);
        while (--nw > 0)
          angel_DebugPrint("%.8lx ", (unsigned long)buffer[i++]);
        angel_DebugPrint("%.8lx\n", (unsigned long)buffer[i++]);
      }
    }
  }
#endif
  return status;
}


/*----------------------------------------------------------------------*/
/*----angel_RDI_CPwrite--------------------------------------------------*/
/*----------------------------------------------------------------------*/

/* Write coprocessor's internal state.  See dbg_cp.h for help. Use
 * ADP_CPwrite.
 */

int angel_RDI_CPwrite(unsigned CPnum, unsigned long mask,
                      ARMword const *buffer)
{
  Packet *packet = NULL;
  int i, j, len, status, reason, OSinfo1, OSinfo2, err, debugID;
  unsigned char *rmap = cpwords[CPnum];
  int n;

  if (rmap == NULL) return RDIError_UnknownCoPro;
  n = rmap[-1];

  TracePrint(("angel_RDI_CPwrite: CPnum=%d mask=%.8lx\n", CPnum, mask));

#ifdef RDI_VERBOSE
 if (rdi_log & 1) {
    for (i = 0, j = 0; j < n ; j++)
       if (mask & (1L << j)) {
          int nw = rmap[j];
          angel_DebugPrint("%2d ", j);
          while (--nw > 0)
             angel_DebugPrint("%.8lx ", (unsigned long)buffer[i++]);
          angel_DebugPrint("%.8lx\n", (unsigned long)buffer[i++]);
       }
 }
#endif

  for (j=i=0; i < n ; i++) /* Count the number of registers. */
    if (mask & (1L << i)) j++;
  packet = DevSW_AllocatePacket(Armsd_BufferSize);
  len = msgbuild(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%c%w",
                 ADP_CPwrite | HtoT, 0,
                 ADP_HandleUnknown, ADP_HandleUnknown, CPnum, mask);
  for(i=0;  i<j; i++)
    len+=msgbuild(BUFFERDATA(packet->pk_buffer) + len, "%w", buffer[i]);
  packet->pk_length = len;
  register_debug_message_handler();
  Adp_ChannelWrite(CI_HADP, packet);    /* Transmit message. */
  reason=ADP_CPwrite | TtoH;
  err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
  DevSW_FreePacket(packet);
  if (err != RDIError_NoError)
    return err;
  else
    return status;
}


/*----------------------------------------------------------------------*/
/*----angel_RDI_pointinq-------------------------------------------------*/
/*----------------------------------------------------------------------*/

/* Do test calls to ADP_SetBreak/ADP_SetWatch to see if resources exist to
   carry out request. */
int angel_RDI_pointinq(ARMword *address, unsigned type, unsigned datatype,
                       ARMword *bound)
{
  Packet *packet = NULL;
  int len, status, reason, OSinfo1, OSinfo2, err=RDIError_NoError;
       /* stop a compiler warning */
  int debugID, pointhandle;
  TracePrint(
      ("angel_RDI_pointinq: address=%.8lx type=%d datatype=%d bound=%.8lx ",
      (unsigned long)*address, type, datatype, (unsigned long)*bound));
       /* for a buffer.  */
  packet = DevSW_AllocatePacket(Armsd_BufferSize);
  len = msgbuild(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%b",
                 ((datatype == 0) ? ADP_SetBreak : ADP_SetWatch) | HtoT, 0,
                 ADP_HandleUnknown, ADP_HandleUnknown, address, type);
  if (datatype == 0)
    len += msgbuild(BUFFERDATA(packet->pk_buffer) + 21, "%w", bound);
  else
    len += msgbuild(BUFFERDATA(packet->pk_buffer) + 21, "%b%w", datatype, bound);

  register_debug_message_handler();
  packet->pk_length = len;
  Adp_ChannelWrite(CI_HADP, packet);
  reason = ((datatype == 0) ? ADP_SetBreak : ADP_SetWatch | TtoH);
  err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
  if (err != RDIError_NoError) {
    DevSW_FreePacket(packet);
    return err;        /* Was there an error? */
  }
  unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w%w%w",
                 &reason, &debugID, &OSinfo1, &OSinfo2, &status,
                 &pointhandle, &address, &bound);
  DevSW_FreePacket(packet);
  return err;
}


/*----------------------------------------------------------------------*/
/*----angel_RDI_setbreak-------------------------------------------------*/
/*----------------------------------------------------------------------*/

/* Set a breakpoint: Use ADP_SetBreak */
int angel_RDI_setbreak(ARMword address, unsigned type, ARMword bound,
                      PointHandle *handle)
{
  int status, reason, OSinfo1, OSinfo2, err, debugID;
  int tmpval, tmpaddr, tmpbnd;
  Packet *packet;
  TracePrint(("angel_RDI_setbreak address=%.8lx type=%d bound=%.8lx \n",
              (unsigned long)address, type, (unsigned long)bound));

  register_debug_message_handler();
  msgsend(CI_HADP, "%w%w%w%w%w%b%w",
          ADP_SetBreak| HtoT, 0,  ADP_HandleUnknown,
          ADP_HandleUnknown, address, type, bound);
  reason = ADP_SetBreak |TtoH;
  err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
  if (err != RDIError_NoError) {
    DevSW_FreePacket(packet);
    return err;         /* Was there an error? */
  }
  /* Work around varargs problem... -sts */
  unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w%w%w",
                 &reason, &debugID, &OSinfo1, &OSinfo2, &status,
                 &tmpval, &tmpaddr, &tmpbnd);
  *handle = tmpval;
  address = tmpaddr;
  bound = tmpbnd;
  DevSW_FreePacket(packet);
  if (status != RDIError_NoError) return status;
  TracePrint(("returns handle %.8lx\n", (unsigned long)*handle));
  return RDIError_NoError;
}


/*----------------------------------------------------------------------*/
/*----angel_RDI_clearbreak-----------------------------------------------*/
/*----------------------------------------------------------------------*/

/* Clear a breakpoint: Use ADP_ClearBreak. */
int angel_RDI_clearbreak(PointHandle handle)
{
  Packet *packet = NULL;
  int status, reason, OSinfo1, OSinfo2, err, debugID;

  TracePrint(("angel_RDI_clearbreak: handle=%.8lx\n", (unsigned long)handle));

  register_debug_message_handler();
  msgsend(CI_HADP, "%w%w%w%w%w",
          ADP_ClearBreak| HtoT, 0,  ADP_HandleUnknown,
          ADP_HandleUnknown, handle);
  reason = ADP_ClearBreak|TtoH;
  err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
  if (err != RDIError_NoError) {
    DevSW_FreePacket(packet);
    angel_DebugPrint("***RECEIVE DEBUG MESSAGE RETURNED ERR = %d.\n", err);
    return err;          /* Was there an error? */
  }
  unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w",  &reason,
                 &debugID, &OSinfo1, &OSinfo2, &status);
  DevSW_FreePacket(packet);
#ifdef DEBUG
  angel_DebugPrint("DEBUG: Clear Break completed OK.\n");
#endif
  return RDIError_NoError;
}


/*----------------------------------------------------------------------*/
/*----angel_RDI_setwatch-------------------------------------------------*/
/*----------------------------------------------------------------------*/

/* Set a watchpoint: use ADP_SetWatch. */
int angel_RDI_setwatch(ARMword address, unsigned type, unsigned datatype,
                       ARMword bound, PointHandle *handle)
{
  Packet *packet = NULL;
  int status, reason, OSinfo1, OSinfo2, err, debugID;

  TracePrint(("angel_RDI_setwatch: address=%.8lx type=%d bound=%.8lx ",
              (unsigned long)address, type, (unsigned long)bound));

  register_debug_message_handler();
  msgsend(CI_HADP, "%w%w%w%w%w%b%b%w",
          ADP_SetWatch| HtoT, 0,  ADP_HandleUnknown,
          ADP_HandleUnknown, address, type, datatype, bound);

  reason = ADP_SetWatch | TtoH;
  err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
  if (err != RDIError_NoError) {
    DevSW_FreePacket(packet);
    return err;        /* Was there an error? */
  }
  unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w%w%w",
                 &reason, &debugID, &OSinfo1, &OSinfo2, &status,
                 handle, &address, &bound);
  DevSW_FreePacket(packet);
  TracePrint(("returns handle %.8lx\n", (unsigned long)*handle));
  return RDIError_NoError;
}

/*----------------------------------------------------------------------*/
/*----angel_RDI_clearwatch-----------------------------------------------*/
/*----------------------------------------------------------------------*/

/* Clear a watchpoint: use ADP_ClearWatch. */
int angel_RDI_clearwatch(PointHandle handle) {

  int status, reason, OSinfo1, OSinfo2, err, debugID;
  Packet *packet = NULL;

  TracePrint(("angel_RDI_clearwatch: handle=%.8lx\n", (unsigned long)handle));

  register_debug_message_handler();
  msgsend(CI_HADP, "%w%w%w%w%w",
          ADP_ClearWatch| HtoT, 0,  ADP_HandleUnknown,
          ADP_HandleUnknown, handle);
  reason = ADP_ClearWatch|TtoH;
  err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
  if (err != RDIError_NoError) {
    DevSW_FreePacket(packet);
    return err;        /* Was there an error? */
  }
  unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w",  &reason, &debugID,
                 &OSinfo1, &OSinfo2, &status);
  DevSW_FreePacket(packet);
  return RDIError_NoError;
}

typedef struct {
  unsigned stopped_reason;
  int stopped_status;
  int data;
} adp_stopped_struct;


int angel_RDI_OnTargetStopping(angel_RDI_TargetStoppedProc *fn,
                               void *arg)
{
  stoppedProcListElement **lptr = &stopped_proc_list;

  /* Find the address of the NULL ptr at the end of the list */
  for (; *lptr!=NULL ; lptr = &((*lptr)->next))
    ; /* Do nothing */

  *lptr = (stoppedProcListElement *) malloc(sizeof(stoppedProcListElement));
  if (*lptr == NULL) return RDIError_OutOfStore;
  (*lptr)->fn = fn;
  (*lptr)->arg = arg;

  return RDIError_NoError;
}

static int CallStoppedProcs(unsigned reason)
{
  stoppedProcListElement *p = stopped_proc_list;
  int err=RDIError_NoError;
  
  for (; p!=NULL ; p=p->next) {
    int local_err = p->fn(reason, p->arg);
    if (local_err != RDIError_NoError) err=local_err;
  }

  return err;
}

/*----------------------------------------------------------------------*/
/*----angel_RDI_execute--------------------------------------------------*/
/*----------------------------------------------------------------------*/

static int HandleStoppedMessage(Packet *packet, void *stateptr) {
  unsigned int err,  reason, debugID, OSinfo1, OSinfo2, count;
  adp_stopped_struct *stopped_info;
  stopped_info = (adp_stopped_struct *) stateptr;
  IGNORE(stateptr);
  count = unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w",
                         &reason, &debugID,
                         &OSinfo1, &OSinfo2,
                         &stopped_info->stopped_reason, &stopped_info->data);
  DevSW_FreePacket(packet);

  if (reason != (ADP_Stopped | TtoH)) {
#ifdef DEBUG
    angel_DebugPrint("Expecting stopped message, got %x", reason);
#endif
    return RDIError_Error;
  }
  else {
    executing = FALSE;
#ifdef DEBUG
    angel_DebugPrint("Received stopped message.\n");
#endif
  }

  err = msgsend(CI_TADP,  "%w%w%w%w%w", (ADP_Stopped | HtoT), 0,
                ADP_HandleUnknown, ADP_HandleUnknown, RDIError_NoError);
#ifdef DEBUG
  angel_DebugPrint("Transmiting stopped acknowledge.\n");
#endif
  if (err != RDIError_NoError) angel_DebugPrint("Transmit failed.\n");
#ifdef DEBUG
  angel_DebugPrint("DEBUG: Stopped reason : %x\n", stopped_info->stopped_reason);
#endif
  switch (stopped_info->stopped_reason) {
  case ADP_Stopped_BranchThroughZero:
    stopped_info->stopped_status = RDIError_BranchThrough0;
    break;
  case ADP_Stopped_UndefinedInstr:
    stopped_info->stopped_status = RDIError_UndefinedInstruction;
    break;
  case ADP_Stopped_SoftwareInterrupt:
    stopped_info->stopped_status = RDIError_SoftwareInterrupt;
    break;
  case ADP_Stopped_PrefetchAbort:
    stopped_info->stopped_status = RDIError_PrefetchAbort;
    break;
  case ADP_Stopped_DataAbort:
    stopped_info->stopped_status = RDIError_DataAbort;
    break;
  case ADP_Stopped_AddressException:
    stopped_info->stopped_status = RDIError_AddressException;
    break;
  case ADP_Stopped_IRQ:
    stopped_info->stopped_status = RDIError_IRQ;
    break;
  case ADP_Stopped_BreakPoint:
    stopped_info->stopped_status = RDIError_BreakpointReached;
    break;
  case ADP_Stopped_WatchPoint:
    stopped_info->stopped_status = RDIError_WatchpointAccessed;
    break;
  case ADP_Stopped_StepComplete:
    stopped_info->stopped_status = RDIError_ProgramFinishedInStep;
    break;
  case ADP_Stopped_RunTimeErrorUnknown:
  case ADP_Stopped_StackOverflow:
  case ADP_Stopped_DivisionByZero:
    stopped_info->stopped_status = RDIError_Error;
    break;
  case ADP_Stopped_FIQ:
    stopped_info->stopped_status = RDIError_FIQ;
    break;
  case ADP_Stopped_UserInterruption:
  case ADP_Stopped_OSSpecific:
    stopped_info->stopped_status = RDIError_UserInterrupt;
    break;
  case ADP_Stopped_ApplicationExit:
    stopped_info->stopped_status = RDIError_NoError;
    break;
  default:
    stopped_info->stopped_status = RDIError_Error;
    break;
  }
  return RDIError_NoError;
}


static void interrupt_target( void )
{
    Packet *packet = NULL;
    int err;
    int reason, debugID, OSinfo1, OSinfo2, status;

#ifdef DEBUG
    angel_DebugPrint("DEBUG: interrupt_target.\n");
#endif

    register_debug_message_handler();
    msgsend(CI_HADP, "%w%w%w%w", ADP_InterruptRequest | HtoT, 0,
                   ADP_HandleUnknown, ADP_HandleUnknown);

    reason = ADP_InterruptRequest |TtoH;
    err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
    DevSW_FreePacket(packet);
#ifdef DEBUG
    angel_DebugPrint("DEBUG: got interrupt ack ok err = %d, status=%i\n",
                     err, status);
#endif

    return;
}

#ifdef TEST_DC_APPL
  extern void test_dc_appl_handler( const DeviceDescr *device,
                                    Packet *packet );
#endif

void angel_RDI_stop_request(void)
{
  stop_request = 1;
}

/* Core functionality for execute and step */
static int angel_RDI_ExecuteOrStep(PointHandle *handle, word type, 
                                   unsigned ninstr)
{
  extern int (*deprecated_ui_loop_hook) (int);
  int err;
  adp_stopped_struct stopped_info;
  void* stateptr = (void *)&stopped_info;
  ChannelCallback HandleStoppedMessageFPtr=(ChannelCallback) HandleStoppedMessage;
  int status, reasoncode, debugID, OSinfo1, OSinfo2;
  Packet *packet = NULL;

  TracePrint(("angel_RDI_ExecuteOrStep\n"));

  err = Adp_ChannelRegisterRead(CI_TADP,
                                HandleStoppedMessageFPtr, stateptr);
  if (err != RDIError_NoError) {
#ifdef DEBUG
    angel_DebugPrint("TADP Register failed.\n");
#endif
    return err;
  }
  /* Set executing TRUE here, as it must be set up before the target has
   * had any chance at all to execute, or it may send its stopped message
   * before we get round to setting executing = TRUE !!!
   */
  executing = TRUE;

  register_debug_message_handler();

#ifdef TEST_DC_APPL
  Adp_Install_DC_Appl_Handler( test_dc_appl_handler );
#endif

#ifdef DEBUG
  angel_DebugPrint("Transmiting %s message.\n",
                   type == ADP_Execute ? "execute": "step");
#endif

  register_debug_message_handler();
  /* Extra ninstr parameter for execute message will simply be ignored */
  err = msgsend(CI_HADP,"%w%w%w%w%w", type | HtoT, 0,
                ADP_HandleUnknown, ADP_HandleUnknown, ninstr);
#if DEBUG
  if (err != RDIError_NoError) angel_DebugPrint("Transmit failed.\n");
#endif

  reasoncode = type | TtoH;
  err = wait_for_debug_message( &reasoncode, &debugID, &OSinfo1, &OSinfo2,
                                &status, &packet );
  if (err != RDIError_NoError)
     return err;
  else if (status != RDIError_NoError)
     return status;

#ifdef DEBUG
  angel_DebugPrint("Waiting for program to finish...\n");
#endif

  interrupt_request = FALSE;
  stop_request = FALSE;
  
  signal(SIGINT, ardi_sigint_handler);
  while( executing )
  {
    if (deprecated_ui_loop_hook)
      deprecated_ui_loop_hook(0);
    
    if (interrupt_request || stop_request)
      {
        interrupt_target();
        interrupt_request = FALSE;
        stop_request = FALSE;
      }
    Adp_AsynchronousProcessing( async_block_on_nothing );
  }
  signal(SIGINT, SIG_IGN);


#ifdef TEST_DC_APPL
  Adp_Install_DC_Appl_Handler( NULL );
#endif

  (void)Adp_ChannelRegisterRead(CI_TADP, NULL, NULL);

  *handle = (PointHandle)stopped_info.data;

  CallStoppedProcs(stopped_info.stopped_reason);

  return stopped_info.stopped_status;
}

/* Request that the target starts executing from the stored CPU state: use
   ADP_Execute. */
int angel_RDI_execute(PointHandle *handle)
{
    return angel_RDI_ExecuteOrStep(handle, ADP_Execute, 0);
}

#ifdef __WATCOMC__
typedef void handlertype(int);

static int interrupted=0;

static void myhandler(int sig) {
  IGNORE(sig);
  interrupted=1;
  signal(SIGINT, myhandler);
}
#endif

/*----------------------------------------------------------------------*/
/*----angel_RDI_step-----------------------------------------------------*/
/*----------------------------------------------------------------------*/

/* Step 'ninstr' through the code: use ADP_Step. */
int angel_RDI_step(unsigned ninstr, PointHandle *handle)
{
    int err = angel_RDI_ExecuteOrStep(handle, ADP_Step, ninstr);
    if (err == RDIError_ProgramFinishedInStep)
       return RDIError_NoError;
    else
       return err;
}


static void SetCPWords(int cpnum, struct Dbg_CoProDesc const *cpd) {
  int i, rmax = 0;
  for (i = 0; i < cpd->entries; i++)
    if (cpd->regdesc[i].rmax > rmax)
      rmax = cpd->regdesc[i].rmax;

  { unsigned char *rmap = (unsigned char *)malloc(rmax + 2);
    *rmap++ = rmax + 1;
    for (i = 0; i < cpd->entries; i++) {
      int r;
      for (r = cpd->regdesc[i].rmin; r <= cpd->regdesc[i].rmax; r++)
        rmap[r] = (cpd->regdesc[i].nbytes+3) / 4;
      }
/*    if (cpwords[cpnum] != NULL) free(cpwords[cpnum]); */
    cpwords[cpnum] = rmap;
  }
}

/*----------------------------------------------------------------------*/
/*----angel_RDI_info-----------------------------------------------------*/
/*----------------------------------------------------------------------*/

/* Use ADP_Info, ADP_Ctrl and ADP_Profile calls to implement these,
   see adp.h for more details. */

static int angel_cc_exists( void )
{
  Packet *packet = NULL;
  int err;
  int reason, debugID, OSinfo1, OSinfo2, subreason, status;

#ifdef DEBUG
  angel_DebugPrint("DEBUG: ADP_ICEB_CC_Exists.\n");
#endif

  if ( angel_RDI_info( RDIInfo_Icebreaker, NULL, NULL ) == RDIError_NoError ) {
    register_debug_message_handler();
    msgsend(CI_HADP, "%w%w%w%w%w", ADP_ICEbreakerHADP | HtoT, 0,
            ADP_HandleUnknown, ADP_HandleUnknown,
            ADP_ICEB_CC_Exists );
    reason = ADP_ICEbreakerHADP |TtoH;
    err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                                &status, &packet);
    if (err != RDIError_NoError) {
      DevSW_FreePacket(packet);
      return err;
    }
    unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w", &reason,
                   &debugID, &OSinfo1, &OSinfo2,  &subreason, &status);
    if (subreason !=  ADP_ICEB_CC_Exists) {
      DevSW_FreePacket(packet);
      return RDIError_Error;
    }
    else
      return status;
  }
  else
    return RDIError_UnimplementedMessage;
}

typedef struct {
  RDICCProc_ToHost *tohost; void *tohostarg;
  RDICCProc_FromHost *fromhost; void *fromhostarg;
  bool registered;
} CCState;
static CCState ccstate = { NULL, NULL, NULL, NULL, FALSE };

static void HandleDCCMessage( Packet *packet, void *stateptr )
{
  unsigned int reason, debugID, OSinfo1, OSinfo2;
  int count;
  CCState *ccstate_p = (CCState *)stateptr;

  count = unpack_message( BUFFERDATA(packet->pk_buffer), "%w%w%w%w",
                          &reason, &debugID, &OSinfo1, &OSinfo2 );
  switch ( reason )
  {
      case ADP_TDCC_ToHost | TtoH:
      {
           /* only handles a single word of data, for now */

          unsigned int nbytes, data;

          unpack_message( BUFFERDATA(packet->pk_buffer)+count, "%w%w",
                          &nbytes, &data );
#ifdef DEBUG
          angel_DebugPrint( "DEBUG: received CC_ToHost message: nbytes %d data %08x.\n",
                  nbytes, data );
#endif
          ccstate_p->tohost( ccstate_p->tohostarg, data );
          msgsend(CI_TTDCC, "%w%w%w%w%w",
                  ADP_TDCC_ToHost | HtoT, debugID, OSinfo1, OSinfo2,
                  RDIError_NoError );
          break;
      }

      case ADP_TDCC_FromHost | TtoH:
      {
           /* only handles a single word of data, for now */

          int valid;
          ARMword data;

          ccstate_p->fromhost( ccstate_p->fromhostarg, &data, &valid );
#ifdef DEBUG
          angel_DebugPrint( "DEBUG: received CC_FromHost message, returning: %08x %s.\n",
                  data, valid ? "VALID" : "INvalid" );
#endif
          msgsend(CI_TTDCC, "%w%w%w%w%w%w%w",
                  ADP_TDCC_FromHost | HtoT, debugID, OSinfo1, OSinfo2,
                  RDIError_NoError, valid ? 1 : 0, data );
          break;
      }

      default:
#ifdef DEBUG
      angel_DebugPrint( "Unexpected TDCC message %08x received\n", reason );
#endif
      break;
  }
  DevSW_FreePacket(packet);
  return;
}

static void angel_check_DCC_handler( CCState *ccstate_p )
{
    int err;

    if ( ccstate_p->tohost != NULL || ccstate_p->fromhost != NULL )
    {
        /* doing DCC, so need a handler */
        if ( ! ccstate_p->registered )
        {
#ifdef DEBUG
            angel_DebugPrint( "Registering handler for TTDCC channel.\n" );
#endif
            err = Adp_ChannelRegisterRead( CI_TTDCC, HandleDCCMessage,
                                           ccstate_p );
            if ( err == adp_ok )
               ccstate_p->registered = TRUE;
#ifdef DEBUG
            else
               angel_DebugPrint( "angel_check_DCC_handler: register failed!\n" );
#endif
        }
    }
    else
    {
        /* not doing DCC, so don't need a handler */
        if ( ccstate_p->registered )
        {
#ifdef DEBUG
            angel_DebugPrint( "Unregistering handler for TTDCC channel.\n" );
#endif
            err = Adp_ChannelRegisterRead( CI_TTDCC, NULL, NULL );
            if ( err == adp_ok )
               ccstate_p->registered = FALSE;
#ifdef DEBUG
            else
               angel_DebugPrint( "angel_check_DCC_handler: unregister failed!\n" );
#endif
        }
    }
}


static int CheckSubMessageReply(int reason, int subreason) {
  Packet *packet = NULL;
  int status, debugID, OSinfo1, OSinfo2;
  int err = RDIError_NoError;
  reason |= TtoH;
  err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                               &status, &packet);
  if (err != RDIError_NoError) {
    status = err;
  } else {
    int sr;
    unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w", &reason, &debugID,
                   &OSinfo1, &OSinfo2, &sr, &status);
    if (subreason != sr) status = RDIError_Error;
  }
  DevSW_FreePacket(packet);
  return status;
}

static int SendSubMessageAndCheckReply(int reason, int subreason) {
  register_debug_message_handler();
  msgsend(CI_HADP, "%w%w%w%w%w", reason | HtoT, 0,
          ADP_HandleUnknown, ADP_HandleUnknown,
          subreason);
  return CheckSubMessageReply(reason, subreason);
}

static int SendSubMessageWordAndCheckReply(int reason, int subreason, ARMword word) {
  register_debug_message_handler();
  msgsend(CI_HADP, "%w%w%w%w%w%w", reason | HtoT, 0,
          ADP_HandleUnknown, ADP_HandleUnknown,
          subreason, word);
  return CheckSubMessageReply(reason, subreason);
}

static int SendSubMessageGetWordAndCheckReply(int reason, int subreason, ARMword *resp) {
  Packet *packet = NULL;
  int status, debugID, OSinfo1, OSinfo2;
  int err = RDIError_NoError;

  register_debug_message_handler();
  msgsend(CI_HADP, "%w%w%w%w%w", reason | HtoT, 0,
          ADP_HandleUnknown, ADP_HandleUnknown,
          subreason);
  reason |= TtoH;
  err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                               &status, &packet);
  if (err != RDIError_NoError) {
    status = err;
  } else {
    int sr;
    unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w%w", &reason, &debugID,
                   &OSinfo1, &OSinfo2,  &sr, &status, resp);
    if (subreason != sr) status = RDIError_Error;
  }
  DevSW_FreePacket(packet);
  return status;
}

static int const hostsex = 1;

int angel_RDI_info(unsigned type, ARMword *arg1, ARMword *arg2) {
  Packet *packet = NULL;
  int len, status, c, reason, subreason, debugID, OSinfo1, OSinfo2;
  int err=RDIError_NoError, cpnum=0;
  struct Dbg_CoProDesc *cpd;
  int count, i;
  unsigned char *bp;

#ifdef DEBUG
  angel_DebugPrint("DEBUG: Entered angel_RDI_info.\n");
#endif
  switch (type) {
  case RDIInfo_Target:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: RDIInfo_Target.\n");
#endif

    register_debug_message_handler();
    msgsend(CI_HADP, "%w%w%w%w%w", ADP_Info | HtoT, 0,
                 ADP_HandleUnknown, ADP_HandleUnknown, ADP_Info_Target);
    reason = ADP_Info |TtoH;
    err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
    if (err != RDIError_NoError) {
      DevSW_FreePacket(packet);
      return err;
    }
    unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w%w%w", &reason,
                   &debugID, &OSinfo1, &OSinfo2,  &subreason, &status,
                   arg1, arg2);
    DevSW_FreePacket(packet);

    if (subreason !=  ADP_Info_Target)
      return RDIError_Error;
    else
      return status;

  case RDISignal_Stop:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: RDISignal_Stop.\n");
    if (interrupt_request)
       angel_DebugPrint("       STILL WAITING to send previous interrupt request\n");
#endif
    interrupt_request = TRUE;
    return RDIError_NoError;

  case RDIInfo_Points:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: RDIInfo_Points.\n");
#endif
    return SendSubMessageGetWordAndCheckReply(ADP_Info, ADP_Info_Points, arg1);

  case RDIInfo_Step:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: RDIInfo_Step.\n");
#endif
    return SendSubMessageGetWordAndCheckReply(ADP_Info, ADP_Info_Step, arg1);

  case RDISet_Cmdline:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: RDISet_Cmdline.\n");
#endif
    if (ardi_commandline != &dummycline)
      free(ardi_commandline);
    ardi_commandline = (char *)malloc(strlen((char*)arg1) + 1) ;
    (void)strcpy(ardi_commandline, (char *)arg1) ;
    return RDIError_NoError;

  case RDIInfo_SetLog:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: RDIInfo_SetLog.\n");
#endif
    rdi_log = (int) *arg1;
    return RDIError_NoError;

  case RDIInfo_Log:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: RDIInfo_Log.\n");
#endif
    *arg1 = rdi_log;
    return RDIError_NoError;


  case RDIInfo_MMU:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: RDIInfo_MMU.\n");
#endif
    return SendSubMessageGetWordAndCheckReply(ADP_Info, ADP_Info_MMU, arg1);

  case RDIInfo_SemiHosting:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: RDIInfo_SemiHosting.\n");
#endif
    return SendSubMessageAndCheckReply(ADP_Info, ADP_Info_SemiHosting);

  case RDIInfo_CoPro:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: RDIInfo_CoPro.\n");
#endif
    return SendSubMessageAndCheckReply(ADP_Info, ADP_Info_CoPro);

  case RDICycles:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: RDICycles.\n");
#endif
    register_debug_message_handler();
    msgsend(CI_HADP, "%w%w%w%w%w", ADP_Info | HtoT, 0,
            ADP_HandleUnknown, ADP_HandleUnknown, ADP_Info_Cycles);
    reason = ADP_Info |TtoH;
    err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
    if (err != RDIError_NoError) {
      DevSW_FreePacket(packet);
      return err;
    }
    unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w", &reason, &debugID,
                 &OSinfo1, &OSinfo2,  &subreason, &status);
    DevSW_FreePacket(packet);
    if (subreason !=  ADP_Info_Cycles)
      return RDIError_Error;
    if (status != RDIError_NoError) return status;
    for (c=0; c<12; c++)
      arg1[c]=GET32LE(BUFFERDATA(packet->pk_buffer)+24+(c*4));
    return status;

  case RDIInfo_DescribeCoPro:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: RDIInfo_DescribeCoPro.\n");
#endif
    cpnum = *(int *)arg1;
    cpd = (struct Dbg_CoProDesc *)arg2;
    packet = DevSW_AllocatePacket(Armsd_BufferSize);
    if (angel_RDI_info(ADP_Info_CoPro, NULL, NULL) != RDIError_NoError)
      return RDIError_Error;
    len = msgbuild(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w", ADP_Info | HtoT, 0,
                   ADP_HandleUnknown, ADP_HandleUnknown,
                   ADP_Info_DescribeCoPro);
    len +=msgbuild(BUFFERDATA(packet->pk_buffer)+20, "%b%b%b%b%b", cpnum,
                   cpd->regdesc[cpnum].rmin, cpd->regdesc[cpnum].rmax,
                   cpd->regdesc[cpnum].nbytes, cpd->regdesc[cpnum].access);
    if ((cpd->regdesc[cpnum].access&0x3) == 0x3){
      len += msgbuild(BUFFERDATA(packet->pk_buffer)+25, "%b%b%b%b%b",
                      cpd->regdesc[cpnum].accessinst.cprt.read_b0,
                      cpd->regdesc[cpnum].accessinst.cprt.read_b1,
                      cpd->regdesc[cpnum].accessinst.cprt.write_b0,
                      cpd->regdesc[cpnum].accessinst.cprt.write_b1, 0xff);
    }
    else {
      len += msgbuild(BUFFERDATA(packet->pk_buffer)+25, "%b%b%b%b%b%",
                      cpd->regdesc[cpnum].accessinst.cpdt.rdbits,
                      cpd->regdesc[cpnum].accessinst.cpdt.nbit,0,0, 0xff);
    }
    register_debug_message_handler();
    packet->pk_length = len;
    Adp_ChannelWrite(CI_HADP, packet); /* Transmit message. */
    reason = ADP_Info |TtoH;
    err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                                &status, &packet);
    if (err != RDIError_NoError) {
      DevSW_FreePacket(packet);
      return err;
    }
    unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w", &reason, &debugID,
                   &OSinfo1, &OSinfo2, &subreason, &status);
    DevSW_FreePacket(packet);
    if (subreason != ADP_Info_DescribeCoPro)
      return RDIError_Error;
    else
      return status;

  case RDIInfo_RequestCoProDesc:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: RDIInfo_RequestCoProDesc.\n");
#endif
    cpnum = *(int *)arg1;
    cpd = (struct Dbg_CoProDesc *)arg2;
    packet = DevSW_AllocatePacket(Armsd_BufferSize);
    len = msgbuild(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w", ADP_Info | HtoT, 0,
                   ADP_HandleUnknown, ADP_HandleUnknown,
                   ADP_Info_RequestCoProDesc);
    len += msgbuild(BUFFERDATA(packet->pk_buffer)+20, "%b", *(int *)arg1);
    packet->pk_length = len;
    register_debug_message_handler();
    Adp_ChannelWrite(CI_HADP, packet); /* Transmit message. */
    reason = ADP_Info |TtoH;
    err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
    if (err != RDIError_NoError) {
      DevSW_FreePacket(packet);
      return err;
    }
    count = unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w", &reason,
                           &debugID, &OSinfo1, &OSinfo2,  &subreason, &status);
    if (subreason !=  ADP_Info_RequestCoProDesc) {
      DevSW_FreePacket(packet);
      return RDIError_Error;
    } else if ( status != RDIError_NoError ) {
      DevSW_FreePacket(packet);
      return status;
    } else {
      bp = BUFFERDATA(packet->pk_buffer)+count;
      for ( i = 0; *bp != 0xFF && i < cpd->entries; ++i ) {
        cpd->regdesc[i].rmin = *bp++;
        cpd->regdesc[i].rmax = *bp++;
        cpd->regdesc[i].nbytes = *bp++;
        cpd->regdesc[i].access = *bp++;
      }
      cpd->entries = i;
      if ( *bp != 0xFF )
        status = RDIError_BufferFull;
      else
        SetCPWords( cpnum, cpd );
      DevSW_FreePacket(packet);
      return status;
    }

  case RDIInfo_GetLoadSize:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_Info_AngelBufferSize.\n");
#endif
    register_debug_message_handler();
    msgsend(CI_HADP, "%w%w%w%w%w", ADP_Info | HtoT, 0,
            ADP_HandleUnknown, ADP_HandleUnknown,
            ADP_Info_AngelBufferSize);
    reason = ADP_Info |TtoH;
    err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
    if (err != RDIError_NoError) {
      DevSW_FreePacket(packet);
      return err;
    }
    unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w", &reason,
                   &debugID, &OSinfo1, &OSinfo2,  &subreason, &status);
    if (subreason !=  ADP_Info_AngelBufferSize) {
      DevSW_FreePacket(packet);
      return RDIError_Error;
    }
    else {
      word defaultsize, longsize;
      unpack_message(BUFFERDATA(packet->pk_buffer)+24, "%w%w",
                     &defaultsize, &longsize);
      *arg1 = longsize - ADP_WriteHeaderSize;   /* space for ADP header */
#ifdef MONITOR_DOWNLOAD_PACKETS
      angel_DebugPrint("DEBUG: ADP_Info_AngelBufferSize: got (%d, %d), returning %d.\n",
             defaultsize, longsize, *arg1);
#endif
      DevSW_FreePacket(packet);
      return status;
    }

  case RDIVector_Catch:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_Ctrl_VectorCatch %lx.\n", *arg1);
#endif
    return SendSubMessageWordAndCheckReply(ADP_Control, ADP_Ctrl_VectorCatch, *arg1);

  case RDISemiHosting_SetState:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_Ctrl_SemiHosting_SetState %lx.\n", *arg1);
#endif
    return SendSubMessageWordAndCheckReply(ADP_Control, ADP_Ctrl_SemiHosting_SetState, *arg1);

  case RDISemiHosting_GetState:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_Ctrl_SemiHosting_GetState.\n");
#endif
    return SendSubMessageGetWordAndCheckReply(ADP_Control, ADP_Ctrl_SemiHosting_GetState, arg1);

  case RDISemiHosting_SetVector:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_Ctrl_SemiHosting_SetVector %lx.\n", *arg1);
#endif
    return SendSubMessageWordAndCheckReply(ADP_Control, ADP_Ctrl_SemiHosting_SetVector, *arg1);

  case RDISemiHosting_GetVector:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_Ctrl_SemiHosting_GetVector.\n");
#endif
    return SendSubMessageGetWordAndCheckReply(ADP_Control, ADP_Ctrl_SemiHosting_GetVector, arg1);

  case RDISemiHosting_SetARMSWI:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_Ctrl_SemiHosting_SetARMSWI.\n");
#endif
    return SendSubMessageWordAndCheckReply(ADP_Control, ADP_Ctrl_SemiHosting_SetARMSWI, *arg1);

  case RDISemiHosting_GetARMSWI:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_Ctrl_SemiHosting_GetARMSWI.\n");
#endif
    return SendSubMessageGetWordAndCheckReply(ADP_Control, ADP_Ctrl_SemiHosting_GetARMSWI, arg1);

  case RDISemiHosting_SetThumbSWI:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_Ctrl_SemiHosting_SetThumbSWI.\n");
#endif
    return SendSubMessageWordAndCheckReply(ADP_Control, ADP_Ctrl_SemiHosting_SetThumbSWI, *arg1);

  case RDISemiHosting_GetThumbSWI:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_Ctrl_SemiHosting_GetThumbSWI.\n");
#endif
    return SendSubMessageGetWordAndCheckReply(ADP_Control, ADP_Ctrl_SemiHosting_GetThumbSWI, arg1);

  case RDIInfo_SetTopMem:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_Ctrl_SetTopMem.\n");
#endif
    return SendSubMessageWordAndCheckReply(ADP_Control, ADP_Ctrl_SetTopMem, *arg1);

  case RDIPointStatus_Watch:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_Ctrl_PointStatus_Watch.\n");
#endif
    register_debug_message_handler();
    msgsend(CI_HADP, "%w%w%w%w%w%w", ADP_Control | HtoT, 0,
            ADP_HandleUnknown, ADP_HandleUnknown,
            ADP_Ctrl_PointStatus_Watch, *arg1 );
    reason = ADP_Control |TtoH;
    err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
    if (err != RDIError_NoError) {
      DevSW_FreePacket(packet);
      return err;
    }
    unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w%w%w", &reason,
                   &debugID, &OSinfo1, &OSinfo2,  &subreason, &status,
                   arg1, arg2);
    if (subreason !=  ADP_Ctrl_PointStatus_Watch) {
      DevSW_FreePacket(packet);
      return RDIError_Error;
    }
    else
      return status;

  case RDIPointStatus_Break:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_Ctrl_PointStatus_Break.\n");
#endif
    register_debug_message_handler();
    msgsend(CI_HADP, "%w%w%w%w%w%w", ADP_Control | HtoT, 0,
            ADP_HandleUnknown, ADP_HandleUnknown,
            ADP_Ctrl_PointStatus_Break, *arg1 );
    reason = ADP_Control |TtoH;
    err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
    if (err != RDIError_NoError) {
      DevSW_FreePacket(packet);
      return err;
    }
    unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w%w%w", &reason,
                   &debugID, &OSinfo1, &OSinfo2,  &subreason, &status,
                   arg1, arg2);
    if (subreason !=  ADP_Ctrl_PointStatus_Break) {
      DevSW_FreePacket(packet);
      return RDIError_Error;
    }
    else
      return status;

  case RDIInfo_DownLoad:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_Ctrl_Download_Supported.\n");
#endif
    return SendSubMessageAndCheckReply(ADP_Control, ADP_Ctrl_Download_Supported);

  case RDIConfig_Count:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_ICEM_ConfigCount.\n");
#endif
    return SendSubMessageGetWordAndCheckReply(ADP_ICEman, ADP_ICEM_ConfigCount, arg1);

  case RDIConfig_Nth:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_ICEM_ConfigNth.\n");
#endif
    register_debug_message_handler();
    msgsend(CI_HADP, "%w%w%w%w%w%w", ADP_ICEman | HtoT, 0,
            ADP_HandleUnknown, ADP_HandleUnknown,
            ADP_ICEM_ConfigNth, *arg1 );
    reason = ADP_ICEman |TtoH;
    err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
    if (err != RDIError_NoError) {
      DevSW_FreePacket(packet);
      return err;
    } else {
      RDI_ConfigDesc *cd = (RDI_ConfigDesc *)arg2;
      unsigned char n;
      len = unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w%w%b",
                           &reason, &debugID,
                           &OSinfo1, &OSinfo2,  &subreason, &status,
                           &cd->version, &n);
      if (subreason !=  ADP_ICEM_ConfigNth) {
        DevSW_FreePacket(packet);
        return RDIError_Error;
      }
      else {
        memcpy( cd->name, BUFFERDATA(packet->pk_buffer)+len, n+1 );
        cd->name[n] = 0;
        return status;
      }
    }

  case RDIInfo_Icebreaker:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_ICEB_Exists.\n");
#endif
    return SendSubMessageAndCheckReply(ADP_ICEbreakerHADP, ADP_ICEB_Exists);

  case RDIIcebreaker_GetLocks:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_ICEB_GetLocks.\n");
#endif
    return SendSubMessageGetWordAndCheckReply(ADP_ICEbreakerHADP, ADP_ICEB_GetLocks, arg1);

  case RDIIcebreaker_SetLocks:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_ICEB_SetLocks.\n");
#endif
    return SendSubMessageWordAndCheckReply(ADP_ICEbreakerHADP, ADP_ICEB_SetLocks, *arg1);

  case RDICommsChannel_ToHost:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_ICEB_CC_Connect_ToHost.\n");
#endif
    if ( angel_cc_exists() == RDIError_NoError ) {

    /*
     * The following three lines of code have to be removed in order to get
     * the Windows Angel Channel Viewer working with the Thumb comms channel.
     * At the moment it allows the ARMSD command line to register a CCIN/CCOUT
     * callback which stops the ACV working!
     */
#ifdef __unix
      ccstate.tohost = (RDICCProc_ToHost *)arg1;
      ccstate.tohostarg = arg2;
      angel_check_DCC_handler( &ccstate );
#endif
#ifdef _WIN32
      
#endif

      register_debug_message_handler();
      msgsend(CI_HADP, "%w%w%w%w%w%b", ADP_ICEbreakerHADP | HtoT, 0,
              ADP_HandleUnknown, ADP_HandleUnknown,
              ADP_ICEB_CC_Connect_ToHost, (arg1 != NULL) );
      return CheckSubMessageReply(ADP_ICEbreakerHADP, ADP_ICEB_CC_Connect_ToHost);
    } else {
      return RDIError_UnimplementedMessage;
    }

  case RDICommsChannel_FromHost:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_ICEB_CC_Connect_FromHost.\n");
#endif
    if ( angel_cc_exists() == RDIError_NoError ) {

      ccstate.fromhost = (RDICCProc_FromHost *)arg1;
      ccstate.fromhostarg = arg2;
      angel_check_DCC_handler( &ccstate );

      register_debug_message_handler();
      msgsend(CI_HADP, "%w%w%w%w%w%b", ADP_ICEbreakerHADP | HtoT, 0,
              ADP_HandleUnknown, ADP_HandleUnknown,
              ADP_ICEB_CC_Connect_FromHost, (arg1 != NULL) );
      return CheckSubMessageReply(ADP_ICEbreakerHADP, ADP_ICEB_CC_Connect_FromHost);
    } else {
      return RDIError_UnimplementedMessage;
    }

  case RDIProfile_Stop:
    return SendSubMessageAndCheckReply(ADP_Profile, ADP_Profile_Stop);

  case RDIProfile_ClearCounts:
    return SendSubMessageAndCheckReply(ADP_Profile, ADP_Profile_ClearCounts);

  case RDIProfile_Start:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: ADP_Profile_Start %ld.\n", (long)*arg1);
#endif
    return SendSubMessageWordAndCheckReply(ADP_Profile, ADP_Profile_Start, *arg1);

  case RDIProfile_WriteMap:
    { RDI_ProfileMap *map = (RDI_ProfileMap *)arg1;
      int32 maplen = map->len,
            offset,
            size;
      int32 chunk = (Armsd_LongBufSize-CHAN_HEADER_SIZE-ADP_ProfileWriteHeaderSize) / sizeof(ARMword);
                     /* Maximum number of words sendable in one message */
      int oldrev = bytesex_reversing();
      int host_little = *(uint8 const *)&hostsex;
#ifdef DEBUG
      angel_DebugPrint("DEBUG: ADP_Profile_WriteMap %ld.\n", maplen);
#endif
      status = RDIError_NoError;
      if (!host_little) {
        bytesex_reverse(1);
        for (offset = 0; offset < maplen; offset++)
          map->map[offset] = bytesex_hostval(map->map[offset]);
      }
      for (offset = 0; offset < maplen; offset += size) {
        unsigned hdrlen;
        size = maplen - offset;
        packet = (Packet *)DevSW_AllocatePacket(Armsd_LongBufSize);
        if (size > chunk) size = chunk;
        hdrlen = msgbuild(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w%w%w",
                          ADP_Profile | HtoT, 0, ADP_HandleUnknown,
                          ADP_HandleUnknown, ADP_Profile_WriteMap,
                          maplen, size, offset);

        /* Copy the data into the packet. */
        memcpy(BUFFERDATA(packet->pk_buffer)+hdrlen,
               &map->map[offset], (size_t)size * sizeof(ARMword));
        packet->pk_length = size * sizeof(ARMword) + hdrlen;
        register_debug_message_handler();
        Adp_ChannelWrite(CI_HADP, packet);
        reason = ADP_Profile | TtoH;
        err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                                     &status, &packet);
        if (err == RDIError_NoError) {
          unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w", &reason,
                         &debugID, &OSinfo1, &OSinfo2, &subreason, &status);
          if (subreason !=  ADP_Profile_WriteMap) {
            err = RDIError_Error;
          }
          DevSW_FreePacket(packet);
        }
        if (err != RDIError_NoError) { status = err; break; }
      }
      if (!host_little) {
        for (offset = 0; offset < maplen; offset++)
          map->map[offset] = bytesex_hostval(map->map[offset]);
        bytesex_reverse(oldrev);
      }
      return status;
    }

  case RDIProfile_ReadMap:
    { int32 maplen = *(int32 *)arg1,
            offset = 0,
            size;
      int32 chunk = (Armsd_BufferSize-CHAN_HEADER_SIZE-ADP_ProfileReadHeaderSize) / sizeof(ARMword);
#ifdef DEBUG
      angel_DebugPrint("DEBUG: ADP_Profile_ReadMap %ld.\n", maplen);
#endif
      status = RDIError_NoError;
      for (offset = 0; offset < maplen; offset += size) {
        size = maplen - offset;
        if (size > chunk) size = chunk;
        register_debug_message_handler();
        msgsend(CI_HADP, "%w%w%w%w%w%w%w", ADP_Profile | HtoT, 0,
                ADP_HandleUnknown, ADP_HandleUnknown,
                ADP_Profile_ReadMap, offset, size);
        reason = ADP_Profile | TtoH;
        err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                                     &status, &packet);
        if (err != RDIError_NoError) return err;
        unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w", &reason,
                       &debugID, &OSinfo1, &OSinfo2, &subreason, &status);
        memcpy(&arg2[offset], BUFFERDATA(packet->pk_buffer)+ADP_ProfileReadHeaderSize,
               size * sizeof(ARMword));
        DevSW_FreePacket(packet);
        if (status != RDIError_NoError) break;
      }
      { int oldrev = bytesex_reversing();
        int host_little = *(uint8 const *)&hostsex;
        if (!host_little) {
          bytesex_reverse(1);
          for (offset = 0; offset < maplen; offset++)
            arg2[offset] = bytesex_hostval(arg2[offset]);
        }
        bytesex_reverse(oldrev);
      }
      return status;
    }

  case RDIInfo_CanTargetExecute:
#ifdef DEBUG
    printf("DEBUG: RDIInfo_CanTargetExecute.\n");
#endif
    return SendSubMessageAndCheckReply(ADP_Info, ADP_Info_CanTargetExecute);

  case RDIInfo_AgentEndianess:
    return SendSubMessageAndCheckReply(ADP_Info, ADP_Info_AgentEndianess);

  default:
#ifdef DEBUG
    angel_DebugPrint("DEBUG: Fell through ADP_Info, default case taken.\n");
    angel_DebugPrint("DEBUG: type = 0x%x.\n", type);
#endif
    if (type & RDIInfo_CapabilityRequest) {
      switch (type & ~RDIInfo_CapabilityRequest) {
        case RDISemiHosting_SetARMSWI:
          return SendSubMessageAndCheckReply(ADP_Info, ADP_Info_ChangeableSHSWI);
        default:
#ifdef DEBUG
          angel_DebugPrint(
          "DEBUG: ADP_Info - Capability Request(%d) - reporting unimplemented \n",
                 type & ~RDIInfo_CapabilityRequest);
#endif
          break;
      }
    }
    return RDIError_UnimplementedMessage;
  }
}


/*----------------------------------------------------------------------*/
/*----angel_RDI_AddConfig------------------------------------------------*/
/*----------------------------------------------------------------------*/

/* Add a configuration: use ADP_ICEM_AddConfig. */
int angel_RDI_AddConfig(unsigned long nbytes) {
  Packet *packet = NULL;
  int status, reason, subreason, debugID, OSinfo1, OSinfo2, err;

#ifdef DEBUG
  angel_DebugPrint("DEBUG: Entered angel_RDI_AddConfig.\n");
#endif
  register_debug_message_handler();
  msgsend(CI_HADP, "%w%w%w%w%w%w", ADP_ICEman | HtoT,
          0, ADP_HandleUnknown, ADP_HandleUnknown,
          ADP_ICEM_AddConfig, nbytes);
  reason=ADP_ICEman | TtoH;
  err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
  if (err != RDIError_NoError) {
    DevSW_FreePacket(packet);
    return -1;
  }
  unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w", &reason, &debugID,
                 &OSinfo1, &OSinfo2, &subreason, &status);
  DevSW_FreePacket(packet);
  if ( subreason != ADP_ICEM_AddConfig )
    return RDIError_Error;
  else
    return status;
}


/*----------------------------------------------------------------------*/
/*----angel_RDI_LoadConfigData-------------------------------------------*/
/*----------------------------------------------------------------------*/

/* Load configuration data: use ADP_Ctrl_Download_Data. */
int angel_RDI_LoadConfigData(unsigned long nbytes, char const *data) {
  Packet *packet = NULL;
  int len, status, reason, subreason, debugID, OSinfo1, OSinfo2, err;

#ifdef DEBUG
  angel_DebugPrint("DEBUG: Entered angel_RDI_LoadConfigData (%d bytes)\n", nbytes);
#endif
#if 0
  if (err = angel_RDI_AddConfig(nbytes) != RDIError_NoError)
    return err;
#endif
  packet = DevSW_AllocatePacket(Armsd_LongBufSize);
  len = msgbuild(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w",
                 ADP_Control | HtoT, 0,
                 ADP_HandleUnknown, ADP_HandleUnknown,
                 ADP_Ctrl_Download_Data, nbytes);
  memcpy(BUFFERDATA(packet->pk_buffer)+len, data, nbytes);
  len += nbytes;
  packet->pk_length = len;
#ifdef DEBUG
  angel_DebugPrint("DEBUG: packet len %d.\n", len);
#endif
  register_debug_message_handler();
  Adp_ChannelWrite(CI_HADP, packet);
  reason=ADP_Control | TtoH;
  err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
  if (err != RDIError_NoError) {
    DevSW_FreePacket(packet);
    return -1;
  }
  unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w", &reason, &debugID,
                 &OSinfo1, &OSinfo2, &subreason, &status);
  DevSW_FreePacket(packet);
  if ( subreason != ADP_Ctrl_Download_Data )
    return RDIError_Error;
  else
    return status;
}


/*----------------------------------------------------------------------*/
/*----angel_RDI_SelectConfig---------------------------------------------*/
/*----------------------------------------------------------------------*/

/* Select a configuration: use ADP_ICEM_SelecConfig.*/
int angel_RDI_SelectConfig(RDI_ConfigAspect aspect, char const *name,
                           RDI_ConfigMatchType matchtype, unsigned versionreq,
                            unsigned *versionp)
{
  Packet *packet = NULL;
  int len, status, reason, subreason, debugID, OSinfo1, OSinfo2, err;

#ifdef DEBUG
  angel_DebugPrint("DEBUG: Entered angel_RDI_SelectConfig.\n");
#endif
  packet = DevSW_AllocatePacket(Armsd_BufferSize);
  len = msgbuild(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%b%b%b%w",
                 ADP_ICEman | HtoT, 0,
                 ADP_HandleUnknown, ADP_HandleUnknown,
                 ADP_ICEM_SelectConfig, aspect, strlen(name),
                 matchtype, versionreq);
  /* copy the name into the buffer */
  memcpy(BUFFERDATA(packet->pk_buffer)+len, name, strlen(name));
  len += strlen(name);
  packet->pk_length = len;
  register_debug_message_handler();
  Adp_ChannelWrite(CI_HADP, packet);
  reason=ADP_ICEman | TtoH;
  err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
  if (err != RDIError_NoError) {
    DevSW_FreePacket(packet);
    return err;
  }
  unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w%w",
                 &reason, &debugID, &OSinfo1, &OSinfo2,
                 &subreason, &status, versionp);
  DevSW_FreePacket(packet);
  if ( subreason != ADP_ICEM_SelectConfig )
    return RDIError_Error;
  else
    return status;
}


/*----------------------------------------------------------------------*/
/*----angel_RDI_LoadAgent------------------------------------------------*/
/*----------------------------------------------------------------------*/

/* Load a new debug agent: use ADP_Ctrl_Download_Agent. */
int angel_RDI_LoadAgent(ARMword dest, unsigned long size,
                       getbufferproc *getb, void *getbarg)
{
  Packet *packet = NULL;
  int  status, reason, subreason, debugID, OSinfo1, OSinfo2, err;
  time_t t;

#if defined(DEBUG) || defined(DEBUG_LOADAGENT)
  angel_DebugPrint("DEBUG: Entered angel_RDI_LoadAgent.\n");
#endif
  register_debug_message_handler();
  msgsend(CI_HADP, "%w%w%w%w%w%w%w", ADP_Control | HtoT,
          0, ADP_HandleUnknown, ADP_HandleUnknown,
          ADP_Ctrl_Download_Agent, dest, size);
  reason=ADP_Control | TtoH;
  err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
  if (err != RDIError_NoError) {
    DevSW_FreePacket(packet);
    return -1;
  }
  unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w", &reason, &debugID,
                 &OSinfo1, &OSinfo2, &subreason, &status);
    DevSW_FreePacket(packet);
  if ( subreason != ADP_Ctrl_Download_Agent )
    return RDIError_Error;
  if ( status != RDIError_NoError )
    return status;

#if defined(DEBUG) || defined(DEBUG_LOADAGENT)
  angel_DebugPrint("DEBUG: starting agent data download.\n");
#endif
  { unsigned long pos = 0, segsize;
    for (; pos < size; pos += segsize) {
      char *b = getb(getbarg, &segsize);
      if (b == NULL) return RDIError_NoError;
      err = angel_RDI_LoadConfigData( segsize, b );
      if (err != RDIError_NoError) return err;
    }
  }
#if defined(DEBUG) || defined(DEBUG_LOADAGENT)
  angel_DebugPrint("DEBUG: finished downloading new agent.\n");
#endif

  /* renegotiate back down */
  err = angel_negotiate_defaults();
  if (err != adp_ok)
     return err;

  /* Output a message to tell the user what is going on.  This is vital
   * when switching from ADP EICE to ADP over JTAG, as then the user
   * has to reset the target board !
   */
  { char msg[256];
    int len=angel_RDI_errmess(msg, 256, adp_new_agent_starting);
    angel_hostif->write(angel_hostif->hostosarg, msg, len);
  }

  /* get new image started */
#if defined(DEBUG) || defined(DEBUG_LOADAGENT)
  angel_DebugPrint("DEBUG: sending start message for new agent.\n");
#endif

  register_debug_message_handler();
  msgsend(CI_HADP, "%w%w%w%w%w%w", ADP_Control | HtoT,
          0, ADP_HandleUnknown, ADP_HandleUnknown,
          ADP_Ctrl_Start_Agent, dest);
  reason=ADP_Control | TtoH;
  err = wait_for_debug_message(&reason, &debugID, &OSinfo1, &OSinfo2,
                              &status, &packet);
  if (err != RDIError_NoError) {
    DevSW_FreePacket(packet);
    return -1;
  }
  unpack_message(BUFFERDATA(packet->pk_buffer), "%w%w%w%w%w%w", &reason,
                 &debugID, &OSinfo1, &OSinfo2, &subreason, &status);
    DevSW_FreePacket(packet);
  if ( subreason != ADP_Ctrl_Start_Agent )
    return RDIError_Error;
  if ( status != RDIError_NoError )
    return status;

  /* wait for image to start up */
  heartbeat_enabled = FALSE;
  t=time(NULL);
  do {
    Adp_AsynchronousProcessing(async_block_on_nothing);
    if ((time(NULL)-t) > 2) {
#ifdef DEBUG
      angel_DebugPrint("DEBUG: no booted message from new image yet.\n");
#endif
      break;
    }
  } while (booted_not_received);
  booted_not_received=1;

  /* Give device driver a chance to do any necessary resyncing with new agent.
   * Only used by etherdrv.c at the moment.
   */
  (void)Adp_Ioctl( DC_RESYNC, NULL );

#if defined(DEBUG) || defined(DEBUG_LOADAGENT)
  angel_DebugPrint("DEBUG: reopening to new agent.\n");
#endif
  err = angel_RDI_open(0, NULL, NULL, NULL);
  switch ( err )
  {
      case RDIError_NoError:
      {
#if defined(DEBUG) || defined(DEBUG_LOADAGENT)
          angel_DebugPrint( "LoadAgent: Open returned RDIError_NoError\n" );
#endif
          break;
      }

      case RDIError_LittleEndian:
      {
#if defined(DEBUG) || defined(DEBUG_LOADAGENT)
          angel_DebugPrint( "LoadAgent: Open returned RDIError_LittleEndian (OK)\n" );
#endif
          err = RDIError_NoError;
          break;
      }

      case RDIError_BigEndian:
      {
#if defined(DEBUG) || defined(DEBUG_LOADAGENT)
          angel_DebugPrint( "LoadAgent: Open returned RDIError_BigEndian (OK)\n" );
#endif
          err = RDIError_NoError;
          break;
      }

      default:
      {
#if defined(DEBUG) || defined(DEBUG_LOADAGENT)
          angel_DebugPrint( "LoadAgent: Open returned %d - unexpected!\n", err );
#endif
          break;
      }
  }
#ifndef NO_HEARTBEAT
  heartbeat_enabled = TRUE;
#endif
  return err;
}

static int angel_RDI_errmess(char *buf, int blen, int errnum) {
  char *s=NULL;
  int n;

  switch (errnum) {
    case adp_malloc_failure:
      s=AdpMess_MallocFailed; break;
    case adp_illegal_args:
      s=AdpMess_IllegalArgs; break;
    case adp_device_not_found:
      s=AdpMess_DeviceNotFound; break;
    case adp_device_open_failed:
      s=AdpMess_DeviceOpenFailed; break;
    case adp_device_already_open:
      s=AdpMess_DeviceAlreadyOpen; break;
    case adp_device_not_open:
      s=AdpMess_DeviceNotOpen; break;
    case adp_bad_channel_id:
      s=AdpMess_BadChannelId; break;
    case adp_callback_already_registered:
      s=AdpMess_CBAlreadyRegd; break;
    case adp_write_busy:
      s=AdpMess_WriteBusy; break;
    case adp_bad_packet:
      s=AdpMess_BadPacket; break;
    case adp_seq_high:
      s=AdpMess_SeqHigh; break;
    case adp_seq_low:
      s=AdpMess_SeqLow; break;
    case adp_timeout_on_open:
      s=AdpMess_TimeoutOnOpen; break;
    case adp_failed:
      s=AdpMess_Failed; break;
    case adp_abandon_boot_wait:
      s=AdpMess_AbandonBootWait; break;
    case adp_late_startup:
      s=AdpMess_LateStartup; break;
    case adp_new_agent_starting:
      s=AdpMess_NewAgentStarting; break;
    default: return 0;
  }
  n=strlen(s);
  if (n>blen-1) n=blen-1;
  memcpy(buf, s, n);
  buf[n++]=0;
  return n;
}

extern const struct RDIProcVec angel_rdi;
const struct RDIProcVec angel_rdi = {
    "ADP",
    angel_RDI_open,
    angel_RDI_close,
    angel_RDI_read,
    angel_RDI_write,
    angel_RDI_CPUread,
    angel_RDI_CPUwrite,
    angel_RDI_CPread,
    angel_RDI_CPwrite,
    angel_RDI_setbreak,
    angel_RDI_clearbreak,
    angel_RDI_setwatch,
    angel_RDI_clearwatch,
    angel_RDI_execute,
    angel_RDI_step,
    angel_RDI_info,
    angel_RDI_pointinq,

    angel_RDI_AddConfig,
    angel_RDI_LoadConfigData,
    angel_RDI_SelectConfig,

    0, /*angel_RDI_drivernames,*/
    0,   /* cpunames */

    angel_RDI_errmess,

    angel_RDI_LoadAgent
};

/* EOF ardi.c */

/* Not strictly necessary, but allows linking this code into armsd. */

struct foo {
    char *name;
    int (*action)();
    char *syntax;
    char **helpmessage;
    int doafterend;
    int dobeforestart;
    int doinmidline;
} hostappl_CmdTable[1] = {{"", NULL}};

void
hostappl_Init()
{
}

int
hostappl_Backstop()
{
  return -30;
}
