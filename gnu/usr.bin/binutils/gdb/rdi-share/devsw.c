/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/* -*-C-*-
 *
 * $Revision: 1.3 $
 *     $Date: 2004/12/27 14:00:54 $
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#include "adp.h"
#include "sys.h"
#include "hsys.h"
#include "rxtx.h"
#include "drivers.h"
#include "buffers.h"
#include "devclnt.h"
#include "adperr.h"
#include "devsw.h"
#include "hostchan.h"
#include "logging.h"

static char *angelDebugFilename = NULL;
static FILE *angelDebugLogFile = NULL;
static int angelDebugLogEnable = 0;

static void openLogFile ()
{
  time_t t;
  
  if (angelDebugFilename == NULL || *angelDebugFilename =='\0')
    return;
  
  angelDebugLogFile = fopen (angelDebugFilename,"a");
  
  if (!angelDebugLogFile)
    {
      fprintf (stderr,"Error opening log file '%s'\n",angelDebugFilename);
      perror ("fopen");
    }
  else
    {
      /* The following line is equivalent to: */
      /* setlinebuf (angelDebugLogFile); */
      setvbuf(angelDebugLogFile, (char *)NULL, _IOLBF, 0);
#if defined(__CYGWIN__)
      setmode(fileno(angelDebugLogFile), O_TEXT);
#endif
    }
  
  time (&t);
  fprintf (angelDebugLogFile,"ADP log file opened at %s\n",asctime(localtime(&t)));
}


static void closeLogFile (void)
{
  time_t t;
  
  if (!angelDebugLogFile)
    return;
  
  time (&t);
  fprintf (angelDebugLogFile,"ADP log file closed at %s\n",asctime(localtime(&t)));
  
  fclose (angelDebugLogFile);
  angelDebugLogFile = NULL;
}

void DevSW_SetLogEnable (int logEnableFlag)
{
  if (logEnableFlag && !angelDebugLogFile)
    openLogFile ();
  else if (!logEnableFlag && angelDebugLogFile)
    closeLogFile ();
  
  angelDebugLogEnable = logEnableFlag;
}


void DevSW_SetLogfile (const char *filename)
{
  closeLogFile ();
  
  if (angelDebugFilename)
    {
      free (angelDebugFilename);
      angelDebugFilename = NULL;
    }
  
  if (filename && *filename)
    {
      angelDebugFilename = strdup (filename);
      if (angelDebugLogEnable)
        openLogFile ();
    }
}


#define WordAt(p)  ((unsigned long) ((p)[0] | ((p)[1]<<8) | ((p)[2]<<16) | ((p)[3]<<24)))

static void dumpPacket(FILE *fp, char *label, struct data_packet *p)
{
  unsigned r;
  int i;
  unsigned char channel;
  
  if (!fp)
    return;
  
  fprintf(fp,"%s [T=%d L=%d] ",label,p->type,p->len);
  for (i=0; i<p->len; ++i)
    fprintf(fp,"%02x ",p->data[i]);
  fprintf(fp,"\n");

  channel = p->data[0];

  r = WordAt(p->data+4);
  
  fprintf(fp,"R=%08x ",r);
  fprintf(fp,"%s ", r&0x80000000 ? "H<-T" : "H->T");

  switch (channel)
    {
     case CI_PRIVATE: fprintf(fp,"CI_PRIVATE: "); break;
     case CI_HADP: fprintf(fp,"CI_HADP: "); break;
     case CI_TADP: fprintf(fp,"CI_TADP: "); break;
     case CI_HBOOT: fprintf(fp,"CI_HBOOT: "); break;
     case CI_TBOOT: fprintf(fp,"CI_TBOOT: "); break;
     case CI_CLIB: fprintf(fp,"CI_CLIB: "); break;
     case CI_HUDBG: fprintf(fp,"CI_HUDBG: "); break;
     case CI_TUDBG: fprintf(fp,"CI_TUDBG: "); break;
     case CI_HTDCC: fprintf(fp,"CI_HTDCC: "); break;
     case CI_TTDCC: fprintf(fp,"CI_TTDCC: "); break;
     case CI_TLOG: fprintf(fp,"CI_TLOG: "); break;
     default:      fprintf(fp,"BadChan: "); break;
    }

  switch (r & 0xffffff)
    {
     case ADP_Booted: fprintf(fp," ADP_Booted "); break;
#if defined(ADP_TargetResetIndication)
     case ADP_TargetResetIndication: fprintf(fp," ADP_TargetResetIndication "); break;
#endif
     case ADP_Reboot: fprintf(fp," ADP_Reboot "); break;
     case ADP_Reset: fprintf(fp," ADP_Reset "); break;
#if defined(ADP_HostResetIndication)
     case ADP_HostResetIndication: fprintf(fp," ADP_HostResetIndication "); break;
#endif      
     case ADP_ParamNegotiate: fprintf(fp," ADP_ParamNegotiate "); break;
     case ADP_LinkCheck: fprintf(fp," ADP_LinkCheck "); break;
     case ADP_HADPUnrecognised: fprintf(fp," ADP_HADPUnrecognised "); break;
     case ADP_Info: fprintf(fp," ADP_Info "); break;
     case ADP_Control: fprintf(fp," ADP_Control "); break;
     case ADP_Read: fprintf(fp," ADP_Read "); break;
     case ADP_Write: fprintf(fp," ADP_Write "); break;
     case ADP_CPUread: fprintf(fp," ADP_CPUread "); break;
     case ADP_CPUwrite: fprintf(fp," ADP_CPUwrite "); break;
     case ADP_CPread: fprintf(fp," ADP_CPread "); break;
     case ADP_CPwrite: fprintf(fp," ADP_CPwrite "); break;
     case ADP_SetBreak: fprintf(fp," ADP_SetBreak "); break;
     case ADP_ClearBreak: fprintf(fp," ADP_ClearBreak "); break;
     case ADP_SetWatch: fprintf(fp," ADP_SetWatch "); break;
     case ADP_ClearWatch: fprintf(fp," ADP_ClearWatch "); break;
     case ADP_Execute: fprintf(fp," ADP_Execute "); break;
     case ADP_Step: fprintf(fp," ADP_Step "); break;
     case ADP_InterruptRequest: fprintf(fp," ADP_InterruptRequest "); break;
     case ADP_HW_Emulation: fprintf(fp," ADP_HW_Emulation "); break;
     case ADP_ICEbreakerHADP: fprintf(fp," ADP_ICEbreakerHADP "); break;
     case ADP_ICEman: fprintf(fp," ADP_ICEman "); break;
     case ADP_Profile: fprintf(fp," ADP_Profile "); break;
     case ADP_InitialiseApplication: fprintf(fp," ADP_InitialiseApplication "); break;
     case ADP_End: fprintf(fp," ADP_End "); break;
     case ADP_TADPUnrecognised: fprintf(fp," ADP_TADPUnrecognised "); break;
     case ADP_Stopped: fprintf(fp," ADP_Stopped "); break;
     case ADP_TDCC_ToHost: fprintf(fp," ADP_TDCC_ToHost "); break;
     case ADP_TDCC_FromHost: fprintf(fp," ADP_TDCC_FromHost "); break;

     case CL_Unrecognised: fprintf(fp," CL_Unrecognised "); break;
     case CL_WriteC: fprintf(fp," CL_WriteC "); break;
     case CL_Write0: fprintf(fp," CL_Write0 "); break;
     case CL_ReadC: fprintf(fp," CL_ReadC "); break;
     case CL_System: fprintf(fp," CL_System "); break;
     case CL_GetCmdLine: fprintf(fp," CL_GetCmdLine "); break;
     case CL_Clock: fprintf(fp," CL_Clock "); break;
     case CL_Time: fprintf(fp," CL_Time "); break;
     case CL_Remove: fprintf(fp," CL_Remove "); break;
     case CL_Rename: fprintf(fp," CL_Rename "); break;
     case CL_Open: fprintf(fp," CL_Open "); break;
     case CL_Close: fprintf(fp," CL_Close "); break;
     case CL_Write: fprintf(fp," CL_Write "); break;
     case CL_WriteX: fprintf(fp," CL_WriteX "); break;
     case CL_Read: fprintf(fp," CL_Read "); break;
     case CL_ReadX: fprintf(fp," CL_ReadX "); break;
     case CL_Seek: fprintf(fp," CL_Seek "); break;
     case CL_Flen: fprintf(fp," CL_Flen "); break;
     case CL_IsTTY: fprintf(fp," CL_IsTTY "); break;
     case CL_TmpNam: fprintf(fp," CL_TmpNam "); break;

     default: fprintf(fp," BadReason "); break;
    }

  i = 20;
  
  if (((r & 0xffffff) == ADP_CPUread ||
       (r & 0xffffff) == ADP_CPUwrite) && (r&0x80000000)==0)
    {
      fprintf(fp,"%02x ", p->data[i]);
      ++i;
    }
  
  for (; i<p->len; i+=4)
    fprintf(fp,"%08x ",WordAt(p->data+i));
  
  fprintf(fp,"\n");
}


/*
 * TODO: this should be adjustable - it could be done by defining
 *       a reason code for DevSW_Ioctl.  It could even be a
 *       per-devicechannel parameter.
 */
static const unsigned int allocsize = ADP_BUFFER_MIN_SIZE;

#define illegalDevChanID(type)  ((type) >= DC_NUM_CHANNELS)

/**********************************************************************/

/*
 *  Function: initialise_read
 *   Purpose: Set up a read request for another packet
 *
 *    Params:
 *      In/Out: ds      State structure to be initialised
 *
 *   Returns:
 *          OK: 0
 *       Error: -1
 */
static int initialise_read(DevSWState *ds)
{
    struct data_packet *dp;

    /*
     * try to claim the structure that will
     * eventually hold the new packet.
     */
    if ((ds->ds_nextreadpacket = DevSW_AllocatePacket(allocsize)) == NULL)
        return -1;

    /*
     * Calls into the device driver use the DriverCall structure: use
     * the buffer we have just allocated, and declare its size.  We
     * are also obliged to clear the driver's context pointer.
     */
    dp = &ds->ds_activeread.dc_packet;
    dp->buf_len = allocsize;
    dp->data = ds->ds_nextreadpacket->pk_buffer;

    ds->ds_activeread.dc_context = NULL;

    return 0;
}

/*
 *  Function: initialise_write
 *   Purpose: Set up a write request for another packet
 *
 *    Params:
 *       Input: packet  The packet to be written
 *
 *              type    The type of the packet
 *
 *      In/Out: dc      The structure to be intialised
 *
 *   Returns: Nothing
 */
static void initialise_write(DriverCall *dc, Packet *packet, DevChanID type)
{
    struct data_packet *dp = &dc->dc_packet;

    dp->len = packet->pk_length;
    dp->data = packet->pk_buffer;
    dp->type = type;

    /*
     * we are required to clear the state structure for the driver
     */
    dc->dc_context = NULL;
}

/*
 *  Function: enqueue_packet
 *   Purpose: move a newly read packet onto the appropriate queue
 *              of read packets
 *
 *    Params:
 *      In/Out: ds      State structure with new packet
 *
 *   Returns: Nothing
 */
static void enqueue_packet(DevSWState *ds)
{
    struct data_packet *dp = &ds->ds_activeread.dc_packet;
    Packet *packet = ds->ds_nextreadpacket;

    /*
     * transfer the length
     */
    packet->pk_length = dp->len;

    /*
     * take this packet out of the incoming slot
     */
    ds->ds_nextreadpacket = NULL;

    /*
     * try to put it on the correct input queue
     */
    if (illegalDevChanID(dp->type))
    {
        /* this shouldn't happen */
        WARN("Illegal type for Rx packet");
        DevSW_FreePacket(packet);
    }
    else
        Adp_addToQueue(&ds->ds_readqueue[dp->type], packet);
}

/*
 *  Function: flush_packet
 *   Purpose: Send a packet to the device driver
 *
 *    Params:
 *       Input: device  The device to be written to
 *
 *      In/Out: dc      Describes the packet to be sent
 *
 *   Returns: Nothing
 *
 * Post-conditions: If the whole packet was accepted by the device
 *                      driver, then dc->dc_packet.data will be
 *                      set to NULL.
 */
static void flush_packet(const DeviceDescr *device, DriverCall *dc)
{
    if (device->DeviceWrite(dc) > 0)
        /*
         * the whole packet was swallowed
         */
        dc->dc_packet.data = NULL;
}

/**********************************************************************/

/*
 * These are the externally visible functions.  They are documented in
 * devsw.h
 */
Packet *DevSW_AllocatePacket(const unsigned int length)
{
    Packet *pk;

    if ((pk = malloc(sizeof(*pk))) == NULL)
    {
        WARN("malloc failure");
        return NULL;
    }

    if ((pk->pk_buffer = malloc(length+CHAN_HEADER_SIZE)) == NULL)
    {
        WARN("malloc failure");
        free(pk);
        return NULL;
    }

    return pk;
}

void DevSW_FreePacket(Packet *pk)
{
    free(pk->pk_buffer);
    free(pk);
}

AdpErrs DevSW_Open(DeviceDescr *device, const char *name, const char *arg,
                   const DevChanID type)
{
    DevSWState *ds;

    /*
     * is this the very first open call for this driver?
     */
    if ((ds = (DevSWState *)(device->SwitcherState)) == NULL)
    {
        /*
         * yes, it is: initialise state
         */
        if ((ds = malloc(sizeof(*ds))) == NULL)
            /* give up */
            return adp_malloc_failure;

        (void)memset(ds, 0, sizeof(*ds));
        device->SwitcherState = (void *)ds;
    }

    /*
     * check that we haven't already been opened for this type
     */
    if ((ds->ds_opendevchans & (1 << type)) != 0)
        return adp_device_already_open;

    /*
     * if no opens have been done for this device, then do it now
     */
    if (ds->ds_opendevchans == 0)
        if (device->DeviceOpen(name, arg) < 0)
            return adp_device_open_failed;

    /*
     * open has finished
     */
    ds->ds_opendevchans |= (1 << type);
    return adp_ok;
}

AdpErrs DevSW_Match(const DeviceDescr *device, const char *name,
                    const char *arg)
{
    return (device->DeviceMatch(name, arg) == -1) ? adp_failed : adp_ok;
}

AdpErrs DevSW_Close (DeviceDescr *device, const DevChanID type)
{
    DevSWState *ds = (DevSWState *)(device->SwitcherState);
    Packet *pk;

    if ((ds->ds_opendevchans & (1 << type)) == 0)
        return adp_device_not_open;

    ds->ds_opendevchans &= ~(1 << type);

    /*
     * if this is the last close for this channel, then inform the driver
     */
    if (ds->ds_opendevchans == 0)
        device->DeviceClose();

    /*
     * release all packets of the appropriate type
     */
    for (pk = Adp_removeFromQueue(&(ds->ds_readqueue[type]));
         pk != NULL;
         pk = Adp_removeFromQueue(&(ds->ds_readqueue[type])))
        DevSW_FreePacket(pk);

    /* Free memory */
    free ((char *) device->SwitcherState);
    device->SwitcherState = 0x0;

    /* that's all */
    return adp_ok;
}

AdpErrs DevSW_Read(const DeviceDescr *device, const DevChanID type,
                   Packet **packet, bool block)
{
  int read_err;
  DevSWState *ds = device->SwitcherState;

    /*
     * To try to get information out of the device driver as
     * quickly as possible, we try and read more packets, even
     * if a completed packet is already available.
     */

    /*
     * have we got a packet currently pending?
     */
  if (ds->ds_nextreadpacket == NULL)
    /*
       * no - set things up
       */
    if (initialise_read(ds) < 0) {
      /*
       * we failed to initialise the next packet, but can
       * still return a packet that has already arrived.
       */
      *packet = Adp_removeFromQueue(&ds->ds_readqueue[type]); 
      return adp_ok;
    }
  read_err = device->DeviceRead(&ds->ds_activeread, block);
  switch (read_err) {
  case 1:
    /*
     * driver has pulled in a complete packet, queue it up
     */
#ifdef RET_DEBUG
    printf("got a complete packet\n");
#endif
    
    if (angelDebugLogEnable)
      dumpPacket(angelDebugLogFile,"rx:",&ds->ds_activeread.dc_packet);

    enqueue_packet(ds);
    *packet = Adp_removeFromQueue(&ds->ds_readqueue[type]);
    return adp_ok;
  case 0:
    /*
     * OK, return the head of the read queue for the given type
     */
    /*    enqueue_packet(ds); */
    *packet = Adp_removeFromQueue(&ds->ds_readqueue[type]);
    return adp_ok;
  case -1:
#ifdef RET_DEBUG
    printf("got a bad packet\n");
#endif
    /* bad packet */
    *packet = NULL;
    return adp_bad_packet;
  default:
    panic("DevSW_Read: bad read status %d", read_err);
  }
  return 0; /* get rid of a potential compiler warning */
}


AdpErrs DevSW_FlushPendingWrite(const DeviceDescr *device)
{
    struct DriverCall *dc;
    struct data_packet *dp;

    dc = &((DevSWState *)(device->SwitcherState))->ds_activewrite;
    dp = &dc->dc_packet;

    /*
     * try to flush any packet that is still being written
     */
    if (dp->data != NULL)
    {
        flush_packet(device, dc);

        /* see if it has gone */
        if (dp->data != NULL)
           return adp_write_busy;
        else
           return adp_ok;
    }
    else
       return adp_ok;
}


AdpErrs DevSW_Write(const DeviceDescr *device, Packet *packet, DevChanID type)
{
    struct DriverCall *dc;
    struct data_packet *dp;

    dc = &((DevSWState *)(device->SwitcherState))->ds_activewrite;
    dp = &dc->dc_packet;

    if (illegalDevChanID(type))
        return adp_illegal_args;

    /*
     * try to flush any packet that is still being written
     */
    if (DevSW_FlushPendingWrite(device) != adp_ok)
       return adp_write_busy;

    /*
     * we can take this packet - set things up, then try to get rid of it
     */
    initialise_write(dc, packet, type);
  
    if (angelDebugLogEnable)
      dumpPacket(angelDebugLogFile,"tx:",&dc->dc_packet);
  
    flush_packet(device, dc);

    return adp_ok;
}

AdpErrs DevSW_Ioctl(const DeviceDescr *device, const int opcode, void *args)
{
    return (device->DeviceIoctl(opcode, args) < 0) ? adp_failed : adp_ok;
}

bool DevSW_WriteFinished(const DeviceDescr *device)
{
    struct DriverCall *dc;
    struct data_packet *dp;

    dc = &((DevSWState *)(device->SwitcherState))->ds_activewrite;
    dp = &dc->dc_packet;

    return (dp == NULL || dp->data == NULL);
}

/* EOF devsw.c */
