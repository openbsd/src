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
 *
 * hostchan.c - Semi Synchronous Host side channel interface for Angel.
 */

#include <stdio.h>

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#else
#  include "winsock.h"
#  include "time.h"
#endif
#include "hsys.h"
#include "host.h"
#include "logging.h"
#include "chandefs.h"
#include "chanpriv.h"
#include "devclnt.h"
#include "buffers.h"
#include "drivers.h"
#include "adperr.h"
#include "devsw.h"
#include "hostchan.h"

#ifndef UNUSED
#define UNUSED(x) (x = x)  /* Silence compiler warnings for unused arguments */
#endif

#define HEARTRATE 5000000

/*
 * list of available drivers, declared in drivers.c
 */
extern DeviceDescr *devices[];

static DeviceDescr *deviceToUse = NULL;

static struct Channel {
    ChannelCallback callback;
    void *callback_state;
} channels[CI_NUM_CHANNELS];

static unsigned char HomeSeq;
static unsigned char OppoSeq;

/*
 * Handler for DC_APPL packets
 */
static DC_Appl_Handler dc_appl_handler = NULL;

/*
 * slots for registered asynchronous processing callback procedures
 */
#define MAX_ASYNC_CALLBACKS 8
static unsigned int             num_async_callbacks = 0;
static Adp_Async_Callback       async_callbacks[MAX_ASYNC_CALLBACKS];

/*
 * writeQueueRoot is the queue of write requests pending acknowledgement
 * writeQueueSend is the queue of pending write requests which will
 * be a subset of the list writeQueueRoot
 */
static Packet *writeQueueRoot = NULL;
static Packet *writeQueueSend = NULL;
static Packet *resend_pkt = NULL;
static int resending = FALSE;

/* heartbeat_enabled is a flag used to indicate whether the heartbeat is
 * currently turned on, heartbeat_enabled will be false in situations
 * where even though a heartbeat is being used it is problematical or
 * dis-advantageous to have it turned on, for instance during the 
 * initial stages of boot up
 */
unsigned int heartbeat_enabled = FALSE;
/* heartbeat_configured is set up by the device driver to indicate whether
 * the heartbeat is being used during this debug session.  In contrast to
 * heartbeat_enabled it must not be changed during a session.  The logic for
 * deciding whether to send a heartbeat is: Is heartbeat_configured for this
 * session? if and only if it is then if heartbeat[is currently]_enabled and
 * we are due to send a pulse then send it 
 */
unsigned int heartbeat_configured = TRUE;

void Adp_initSeq( void ) {
  Packet *tmp_pkt = writeQueueSend;

  HomeSeq = 0;
  OppoSeq = 0;
  if ( writeQueueSend != NULL) {
    while (writeQueueSend->pk_next !=NULL) {
      tmp_pkt = writeQueueSend;
      writeQueueSend = tmp_pkt->pk_next;
      DevSW_FreePacket(tmp_pkt);
    }
  }
  tmp_pkt = writeQueueRoot;
  if ( writeQueueRoot == NULL)
    return;

  while (writeQueueRoot->pk_next !=NULL) {
    tmp_pkt = writeQueueRoot;
    writeQueueRoot = tmp_pkt->pk_next;
    DevSW_FreePacket(tmp_pkt);
  }
  return;
}

/**********************************************************************/

/*
 *  Function: DummyCallback
 *   Purpose: Default callback routine to handle unexpected input
 *              on a channel
 *
 *    Params:
 *       Input: packet  The received packet
 *
 *              state   Contains nothing of significance
 *
 *   Returns: Nothing
 */
static void DummyCallback(Packet *packet, void *state)
{
    ChannelID chan;
    const char fmt[] = "Unexpected read on channel %u, length %d\n";
    char fmtbuf[sizeof(fmt) + 24];

    UNUSED(state);

    chan = *(packet->pk_buffer);
    sprintf(fmtbuf, fmt, chan, packet->pk_length);
    printf(fmtbuf);

    /*
     * junk this packet
     */
    DevSW_FreePacket(packet);
}

/*
 *  Function: BlockingCallback
 *   Purpose: Callback routine used to implement a blocking read call
 *
 *    Params:
 *       Input: packet  The received packet.
 *
 *      Output: state   Address of higher level's pointer to the received
 *                      packet.
 *
 *   Returns: Nothing
 */
static void BlockingCallback(Packet *packet, void *state)
{
    /*
     * Pass the packet back to the caller which requested a packet
     * from this channel.  This also flags the completion of the I/O
     * request to the blocking read call.
     */
    *((Packet **)state) = packet;
}

/*
 *  Function: FireCallback
 *   Purpose: Pass received packet along to the callback routine for
 *              the appropriate channel
 *
 *    Params:
 *       Input: packet  The received packet.
 *
 *   Returns: Nothing
 *
 * Post-conditions: The Target-to-Host sequence number for the channel
 *                      will have been incremented.
 */
static void FireCallback(Packet *packet)
{
    ChannelID chan;
    struct Channel *ch;

    /*
     * is this a sensible channel number?
     */
    chan = *(packet->pk_buffer);
    if (invalidChannelID(chan))
    {
        printf("ERROR: invalid ChannelID received from target\n");

        /*
         * free the packet's resources, 'cause no-one else will
         */
        DevSW_FreePacket(packet);
        return;
    }

    /*
     * looks OK - increment sequence number, and pass packet to callback
     */
    ch = channels + chan;
    (ch->callback)(packet, ch->callback_state);
}

/**********************************************************************/

/*
 * These are the externally visible functions.  They are documented
 * in hostchan.h
 */
void Adp_addToQueue(Packet **head, Packet *newpkt)
{
    /*
     * this is a bit of a hack
     */
    Packet *pk;

    /*
     * make sure that the hack we are about to use will work as expected
     */
    ASSERT(&(((Packet *)0)->pk_next) == 0, "bad struct Packet layout");

#if defined(DEBUG) && 0
    printf("Adp_addToQueue(%p, %p)\n", head, newpkt);
#endif

    /*
     * here's the hack - it relies upon the next
     * pointer being at the start of Packet.
     */
    pk = (Packet *)(head);

    /*
     * skip to the end of the queue
     */
    while (pk->pk_next != NULL)
        pk = pk->pk_next;

    /*
     * now add the new element
     */
    newpkt->pk_next = NULL;
    pk->pk_next = newpkt;
}

Packet *Adp_removeFromQueue(Packet **head)
{
    struct Packet *pk;

    pk = *head;

    if (pk != NULL)
        *head = pk->pk_next;

    return pk;
}

void Adp_SetLogEnable(int logEnableFlag)
{
  DevSW_SetLogEnable(logEnableFlag);
}

void Adp_SetLogfile(const char *filename)
{
  DevSW_SetLogfile(filename);
}

AdpErrs Adp_OpenDevice(const char *name, const char *arg,
                       unsigned int heartbeat_on)
{
    int i;
    AdpErrs retc;
    ChannelID chan;

#ifdef DEBUG
    printf("Adp_OpenDevice(%s, %s)\n", name, arg ? arg : "<NULL>");
#endif

    heartbeat_configured = heartbeat_on;
    if (deviceToUse != NULL)
        return adp_device_already_open;

    for (i = 0; (deviceToUse = devices[i]) != NULL; ++i)
        if (DevSW_Match(deviceToUse, name, arg) == adp_ok)
            break;

    if (deviceToUse == NULL)
        return adp_device_not_found;

    /*
     * we seem to have found a suitable device driver, so try to open it
     */
    if ((retc = DevSW_Open(deviceToUse, name, arg, DC_DBUG)) != adp_ok)
    {
        /* we don't have a device to use */
        deviceToUse = NULL;
        return retc;
    }

    /*
     * there is no explicit open on channels any more, so
     * initialise state for all channels.
     */
    for (chan = 0; chan < CI_NUM_CHANNELS; ++chan)
    {
        struct Channel *ch = channels + chan;

        ch->callback = DummyCallback;
        ch->callback_state = NULL;
        OppoSeq = 0;
        HomeSeq = 0;
    }

    return adp_ok;
}

AdpErrs Adp_CloseDevice(void)
{
    AdpErrs retc;

#ifdef DEBUG
    printf("Adp_CloseDevice\n");
#endif

    if (deviceToUse == NULL)
        return adp_device_not_open;

    heartbeat_enabled = FALSE;

    retc = DevSW_Close(deviceToUse, DC_DBUG);

    /*
     * we have to clear deviceToUse, even when the lower layers
     * faulted the close, otherwise the condition will never clear
     */
    if (retc != adp_ok)
        WARN("DevSW_Close faulted the call");

    deviceToUse = NULL;
    return retc;
}

AdpErrs Adp_Ioctl(int opcode, void *args)
{
#ifdef DEBUG
    printf("Adp_Ioctl\n");
#endif

    if (deviceToUse == NULL)
        return adp_device_not_open;

    return DevSW_Ioctl(deviceToUse, opcode, args);
}

AdpErrs Adp_ChannelRegisterRead(const ChannelID chan,
                                const ChannelCallback cbfunc,
                                void *cbstate)
{
#ifdef DEBUG
    printf("Adp_ChannelRegisterRead(%d, %p, %x)\n", chan, cbfunc, cbstate);
#endif

    if (deviceToUse == NULL)
        return adp_device_not_open;

    if (invalidChannelID(chan))
        return adp_bad_channel_id;

    if (cbfunc == NULL)
    {
        channels[chan].callback = DummyCallback;
        channels[chan].callback_state = NULL;
    }
    else
    {
        channels[chan].callback = cbfunc;
        channels[chan].callback_state = cbstate;
    }

    return adp_ok;
}

AdpErrs Adp_ChannelRead(const ChannelID chan, Packet **packet)
{
    struct Channel *ch;

#ifdef DEBUG
    printf("Adp_ChannelRead(%d, %x)\n", chan, *packet);
#endif

    if (deviceToUse == NULL)
        return adp_device_not_open;

    if (invalidChannelID(chan))
        return adp_bad_channel_id;

    /*
     * if a callback has already been registered for this
     * channel, then we do not allow this blocking read.
     */
    ch = channels + chan;
    if (ch->callback != DummyCallback)
        return adp_callback_already_registered;

    /*
     * OK, use our own callback to wait for a packet to arrive
     * on this channel
     */
    ch->callback = BlockingCallback;
    ch->callback_state = packet;
    *packet = NULL;

    /*
     * keep polling until a packet appears for this channel
     */
    while (((volatile Packet *)(*packet)) == NULL)
        /*
         * this call will block until a packet is read on any channel
         */
        Adp_AsynchronousProcessing(async_block_on_read);

    /*
     * OK, the packet has arrived: clear the callback
     */
    ch->callback = DummyCallback;
    ch->callback_state = NULL;

    return adp_ok;
}

static AdpErrs ChannelWrite(
    const ChannelID chan, Packet *packet, AsyncMode mode)
{
    struct Channel *ch;
    unsigned char *cptr;

#ifdef DEBUG
    printf( "Adp_ChannelWrite(%d, %x)\n", chan, packet );
#endif

    if (deviceToUse == NULL)
        return adp_device_not_open;

    if (invalidChannelID(chan))
        return adp_bad_channel_id;

    /*
     * fill in the channels header at the start of this buffer
     */
    ch = channels + chan;
    cptr = packet->pk_buffer;
    *cptr++ = chan;
    *cptr = 0;
    packet->pk_length += CHAN_HEADER_SIZE;

    /*
     * OK, add this packet to the write queue, and try to flush it out
     */

    Adp_addToQueue(&writeQueueSend, packet);
    Adp_AsynchronousProcessing(mode);

    return adp_ok;
}

AdpErrs Adp_ChannelWrite(const ChannelID chan, Packet *packet) {
  return ChannelWrite(chan, packet, async_block_on_write);
}

AdpErrs Adp_ChannelWriteAsync(const ChannelID chan, Packet *packet) {
  return ChannelWrite(chan, packet, async_block_on_nothing);
}

static AdpErrs send_resend_msg(DeviceID devid) {

  /*
   * Send a resend message, usually in response to a bad packet or
   * a resend request */
  Packet * packet;
  packet = DevSW_AllocatePacket(CF_DATA_BYTE_POS);
  packet->pk_buffer[CF_CHANNEL_BYTE_POS] = CI_PRIVATE;
  packet->pk_buffer[CF_HOME_SEQ_BYTE_POS] = HomeSeq;
  packet->pk_buffer[CF_OPPO_SEQ_BYTE_POS] = OppoSeq;
  packet->pk_buffer[CF_FLAGS_BYTE_POS] = CF_RELIABLE | CF_RESEND;
  packet->pk_length = CF_DATA_BYTE_POS;
  return DevSW_Write(deviceToUse, packet, devid);
}

static AdpErrs check_seq(unsigned char msg_home, unsigned char msg_oppo) {
  Packet *tmp_pkt;

  UNUSED(msg_oppo);
  /* 
   * check if we have got an ack for anything and if so remove it from the
   * queue
   */
  if (msg_home == (unsigned char)(OppoSeq+1)) {
    /*
     * arrived in sequence can increment our opposing seq number and remove
     * the relevant packet from our queue
     * check that the packet we're going to remove really is the right one
     */
    tmp_pkt = writeQueueRoot;
    while ((tmp_pkt->pk_next != NULL) &&
           (tmp_pkt->pk_next->pk_buffer[CF_HOME_SEQ_BYTE_POS]
            != OppoSeq)){
      tmp_pkt = tmp_pkt->pk_next;
    }
    OppoSeq++;
    if (tmp_pkt->pk_next == NULL) {
#ifdef DEBUG
      printf("trying to remove a non existant packet\n");
#endif
      return adp_bad_packet;
    }
    else {
      Packet *tmp = tmp_pkt->pk_next;
#ifdef RET_DEBUG
      printf("removing a packet from the root queue\n");
#endif
      tmp_pkt->pk_next = tmp_pkt->pk_next->pk_next;
      /* remove the appropriate packet */
      DevSW_FreePacket(tmp);
    return adp_ok;
    }
  }
  else if (msg_home < (unsigned char) (OppoSeq+1)){
    /* already received this message */
#ifdef RET_DEBUG
    printf("sequence numbers low\n");
#endif   
    return adp_seq_low;
  }
  else {  /* we've missed something */
#ifdef RET_DEBUG
    printf("sequence numbers high\n");
#endif   
    return adp_seq_high;
  }
}

static unsigned long tv_diff(const struct timeval *time_now, 
                             const struct timeval *time_was)
{
    return (  ((time_now->tv_sec * 1000000) + time_now->tv_usec)
            - ((time_was->tv_sec * 1000000) + time_was->tv_usec) );
}

#if !defined(__unix) && !defined(__CYGWIN__)
static void gettimeofday( struct timeval *time_now, void *dummy )
{
    time_t t = clock();
    UNUSED(dummy);
    time_now->tv_sec = t/CLOCKS_PER_SEC;
    time_now->tv_usec = (t%CLOCKS_PER_SEC)*(1000000/CLOCKS_PER_SEC);
}
#endif

static AdpErrs pacemaker(void)
{
  Packet *packet;

  packet = DevSW_AllocatePacket(CF_DATA_BYTE_POS);
  if (packet == NULL) {
    printf("ERROR: could not allocate a packet in pacemaker()\n");
    return adp_malloc_failure;
  }
  packet->pk_buffer[CF_CHANNEL_BYTE_POS] = CI_PRIVATE;
  packet->pk_buffer[CF_HOME_SEQ_BYTE_POS] = HomeSeq;
  packet->pk_buffer[CF_OPPO_SEQ_BYTE_POS] = OppoSeq;
  packet->pk_buffer[CF_FLAGS_BYTE_POS] = CF_RELIABLE | CF_HEARTBEAT;
  packet->pk_length = CF_DATA_BYTE_POS;
  return DevSW_Write(deviceToUse, packet, DC_DBUG);
}  

#ifdef FAKE_BAD_LINE_RX
static AdpErrs fake_bad_line_rx( const Packet *const packet, AdpErrs adp_err )
{
    static unsigned int bl_num = 0;

    if (     (packet != NULL)
          && (bl_num++ >= 20 )
          && ((bl_num % FAKE_BAD_LINE_RX) == 0))
    {
        printf("DEBUG: faking a bad packet\n");
        return adp_bad_packet;
    }
    return adp_err;
}
#endif /* def FAKE_BAD_LINE_RX */

#ifdef FAKE_BAD_LINE_TX
static unsigned char tmp_ch;

static void fake_bad_line_tx( void )
{
    static unsigned int bl_num = 0;

    /* give the thing a chance to boot then try corrupting stuff */
    if ( (bl_num++ >= 20) && ((bl_num % FAKE_BAD_LINE_TX) == 0)) 
    {
        printf("DEBUG: faking a bad packet for tx\n");
        tmp_ch = writeQueueSend->pk_buffer[CF_FLAGS_BYTE_POS];
        writeQueueSend->pk_buffer[CF_FLAGS_BYTE_POS] = 77;
    }
}

static void unfake_bad_line_tx( void )
{
    static unsigned int bl_num = 0;

    /*
     * must reset the packet so that its not corrupted when we
     *  resend it 
     */   
    if ( (bl_num >= 20) && ((bl_num % FAKE_BAD_LINE_TX) != 0))
    {
        writeQueueSend->pk_buffer[CF_FLAGS_BYTE_POS] = tmp_ch;
    }
}
#endif /* def FAKE_BAD_LINE_TX */

/*
 * NOTE: we are assuming that a resolution of microseconds will
 * be good enough for the purporses of the heartbeat.  If this proves
 * not to be the case then we may need a rethink, possibly using
 * [get,set]itimer
 */
static struct timeval time_now;
static struct timeval time_lastalive;

static void async_process_dbug_read( const AsyncMode mode,
                                     bool *const finished  )
{
    Packet *packet;
    unsigned int msg_home, msg_oppo;
    AdpErrs adp_err;

    adp_err = DevSW_Read(deviceToUse, DC_DBUG, &packet,
                         mode == async_block_on_read    );

#ifdef FAKE_BAD_LINE_RX
    adp_err = fake_bad_line_rx( packet, adp_err );
#endif

    if (adp_err == adp_bad_packet) {
        /* We got a bad packet, ask for a resend, send a resend message */
#ifdef DEBUG
        printf("received a bad packet\n");
#endif
        send_resend_msg(DC_DBUG);
    }
    else if (packet != NULL)
    {
        /* update the heartbeat clock */
        gettimeofday(&time_lastalive, NULL);

            /*
             * we got a live one here - were we waiting for it?
             */
        if (mode == async_block_on_read)
           /* not any more */
           *finished = TRUE;
#ifdef RETRANS

        if (packet->pk_length < CF_DATA_BYTE_POS) {
            /* we've got a packet with no header information! */
            printf("ERROR: packet with no transport header\n");
            send_resend_msg(DC_DBUG);
        }
        else {
#ifdef RET_DEBUG
            unsigned int c;
#endif
            /*
             * TODO: Check to see if its acknowledgeing anything, remove
             * those packets it is from the queue.  If its a retrans add the
             * packets to the queue
             */
            msg_home = packet->pk_buffer[CF_HOME_SEQ_BYTE_POS];
            msg_oppo = packet->pk_buffer[CF_OPPO_SEQ_BYTE_POS];
#ifdef RET_DEBUG
            printf("msg seq numbers are hseq 0x%x oseq 0x%x\n",
                   msg_home, msg_oppo);
            for (c=0;c<packet->pk_length;c++)
               printf("%02.2x", packet->pk_buffer[c]);
            printf("\n");
#endif
            /* now was it a resend request? */
            if ((packet->pk_buffer[CF_FLAGS_BYTE_POS]) 
                & CF_RESEND) {
                /* we've been asked for a resend so we had better resend */
                /*
                 * I don't think we can use a resend as acknowledgement for
                 * anything so lets not do this for the moment
                 * check_seq(msg_home, msg_oppo);
                 */
#ifdef RET_DEBUG
                printf("received a resend request\n");
#endif
                if (HomeSeq != msg_oppo) {
                    int found = FALSE;
                    /* need to resend from msg_oppo +1 upwards */
                    DevSW_FreePacket(packet);
                    resending = TRUE;
                    /* find the correct packet to resend from */
                    packet = writeQueueRoot;
                    while (((packet->pk_next) != NULL) && !found) {
                        if ((packet->pk_buffer[CF_OPPO_SEQ_BYTE_POS])
                            != msg_oppo+1) {
                            resend_pkt = packet;
                            found = TRUE;
                        }
                        packet = packet->pk_next;
                    }
                    if (!found) {
                        panic("trying to resend non-existent packets\n");
                    }
                }
                else if (OppoSeq != msg_home) {
                    /* 
                     * send a resend request telling the target where we think
                     * the world is at 
                     */
                    DevSW_FreePacket(packet);
                    send_resend_msg(DC_DBUG);
                }
            }
            else {
                /* not a resend request, lets check the sequence numbers */
                
                if ((packet->pk_buffer[CF_CHANNEL_BYTE_POS] != CI_HBOOT) &&
                    (packet->pk_buffer[CF_CHANNEL_BYTE_POS] != CI_TBOOT)) {
                    adp_err = check_seq(msg_home, msg_oppo);
                    if (adp_err == adp_seq_low) {
                        /* we have already received this packet so discard */
                        DevSW_FreePacket(packet);
                    }
                    else if (adp_err == adp_seq_high) {
                        /*
                         * we must have missed a packet somewhere, discard this 
                         * packet and tell the target where we are
                         */
                        DevSW_FreePacket(packet);
                        send_resend_msg(DC_DBUG);
                    }
                    else
                       /*
                        * now pass the packet to whoever is waiting for it
                        */
                       FireCallback(packet);
                }
                else
                   FireCallback(packet);
            }
        }
#else
        /*
             * now pass the packet to whoever is waiting for it
             */
        FireCallback(packet);
#endif
    }
}

static void async_process_appl_read(void)
{
    Packet *packet;
    AdpErrs adp_err;

    /* see if there is anything for the DC_APPL channel */
    adp_err = DevSW_Read(deviceToUse, DC_APPL, &packet, FALSE);

    if (adp_err == adp_ok && packet != NULL)
    {
        /* got an application packet on a shared device */

#ifdef DEBUG
        printf("GOT DC_APPL PACKET: len %d\nData: ", packet->pk_length);
        {
            unsigned int c;
            for ( c = 0; c < packet->pk_length; ++c )
               printf( "%02X ", packet->pk_buffer[c] );
        }
        printf("\n");
#endif

        if (dc_appl_handler != NULL)
        {
            dc_appl_handler( deviceToUse, packet );
        }
        else
        {
            /* for now, just free it!! */
#ifdef DEBUG
            printf("no handler - dropping DC_APPL packet\n");
#endif
            DevSW_FreePacket( packet );
        }
    }
}

static void async_process_write( const AsyncMode mode,
                                 bool *const finished  )
{
    Packet *packet;

#ifdef DEBUG
    static unsigned int num_written = 0;
#endif

    /*
     * NOTE: here we rely in the fact that any packet in the writeQueueSend
     * section of the queue will need its sequence number setting up while
     * and packet in the writeQueueRoot section will have its sequence
     * numbers set up from when it was first sent so we can easily look
     * up the packet numbers when(if) we want to resend the packet.
     */

#ifdef DEBUG
    if (writeQueueSend!=NULL)
       printf("written 0x%x\n",num_written += writeQueueSend->pk_length);
#endif
    /*
     * give the switcher a chance to complete any partial writes
     */
    if (DevSW_FlushPendingWrite(deviceToUse) == adp_write_busy)
    {
        /* no point trying a new write */
        return;
    }
      
    /*
     * now see whether there is anything to write
     */
    packet = NULL;
    if (resending) {
        packet = resend_pkt;
#ifdef RET_DEBUG
        printf("resending hseq 0x%x oseq 0x%x\n", 
               packet->pk_buffer[CF_HOME_SEQ_BYTE_POS],
               packet->pk_buffer[CF_OPPO_SEQ_BYTE_POS]);
#endif
    }
    else if (writeQueueSend != NULL) {
#ifdef RETRANS
        /* set up the sequence number on the packet */
        packet = writeQueueSend;
        HomeSeq++;
        (writeQueueSend->pk_buffer[CF_OPPO_SEQ_BYTE_POS])
            = OppoSeq;
        (writeQueueSend->pk_buffer[CF_HOME_SEQ_BYTE_POS])
            = HomeSeq;
        (writeQueueSend->pk_buffer[CF_FLAGS_BYTE_POS])
            = CF_RELIABLE;
# ifdef RET_DEBUG
        printf("sending packet with hseq 0x%x oseq 0x%x\n",
               writeQueueSend->pk_buffer[CF_HOME_SEQ_BYTE_POS],
               writeQueueSend->pk_buffer[CF_OPPO_SEQ_BYTE_POS]);
# endif
#endif /* RETRANS */
    }

    if (packet != NULL) {
        AdpErrs dev_err;

#ifdef FAKE_BAD_LINE_TX
        fake_bad_line_tx();
#endif

        dev_err = DevSW_Write(deviceToUse, packet, DC_DBUG);
        if (dev_err == adp_ok) {
#ifdef RETRANS
            if (resending) {
                /* check to see if we've recovered yet */
                if ((packet->pk_next) == NULL){
# ifdef RET_DEBUG
                    printf("we have recovered\n");
# endif
                    resending = FALSE;
                }
                else {
                    resend_pkt = resend_pkt->pk_next;
                }
            }
            else {
                /* 
                 * move the packet we just sent from the send queue to the root
                 */
                Packet *tmp_pkt, *tmp;

# ifdef FAKE_BAD_LINE_TX
                unfake_bad_line_tx();
# endif

                tmp_pkt = writeQueueSend;
                writeQueueSend = writeQueueSend->pk_next;
                tmp_pkt->pk_next = NULL;
                if (writeQueueRoot == NULL)
                   writeQueueRoot = tmp_pkt;
                else {
                    tmp = writeQueueRoot;
                    while (tmp->pk_next != NULL) {
                        tmp = tmp->pk_next;
                    }
                    tmp->pk_next = tmp_pkt;
                }
            }
#else  /* not RETRANS */
            /*
             * switcher has taken the write, so remove it from the
             * queue, and free its resources
             */
            DevSW_FreePacket(Adp_removeFromQueue(&writeQueueSend));
#endif /* if RETRANS ... else ... */

            if (mode == async_block_on_write)
               *finished = DevSW_WriteFinished(deviceToUse);

        } /* endif write ok */
    }
    else /* packet == NULL */
    {
        if (mode == async_block_on_write)
           *finished = DevSW_WriteFinished(deviceToUse);
    }
}

static void async_process_heartbeat( void )
{
    /* check to see whether we need to send a heartbeat */
    gettimeofday(&time_now, NULL);

    if (tv_diff(&time_now, &time_lastalive) >= HEARTRATE)
    {
        /*
         * if we've not booted then don't do send a heartrate the link
         * must be reliable enough for us to boot without any clever stuff,
         * if we can't do this then theres little chance of the link staying
         * together even with the resends etc
         */
        if (heartbeat_enabled) {
            gettimeofday(&time_lastalive, NULL);
            pacemaker();
        }
    }
}

static void async_process_callbacks( void )
{
    /* call any registered asynchronous callbacks */
    unsigned int i;
    for ( i = 0; i < num_async_callbacks; ++i )
       async_callbacks[i]( deviceToUse, &time_now );
}

void Adp_AsynchronousProcessing(const AsyncMode mode)
{
    bool finished = FALSE;
#ifdef DEBUG
    unsigned int wc = 0, dc = 0, ac = 0, hc = 0;
# define INC_COUNT(x) ((x)++)
#else
# define INC_COUNT(x)
#endif

    if ((time_lastalive.tv_sec == 0) && (time_lastalive.tv_usec == 0)) {
      /* first time through, needs initing */
      gettimeofday(&time_lastalive, NULL);
    }

    /* main loop */
    do
    {
        async_process_write( mode, &finished );
        INC_COUNT(wc);

        if ( ! finished && mode != async_block_on_write )
        {
            async_process_dbug_read( mode, &finished );
            INC_COUNT(dc);
        }

        if ( ! finished && mode != async_block_on_write )
        {
           async_process_appl_read();
           INC_COUNT(ac);
        }

        if ( ! finished )
        {
          if (heartbeat_configured)
            async_process_heartbeat();
          async_process_callbacks();
          INC_COUNT(hc);
        }

    } while (!finished && mode != async_block_on_nothing);

#ifdef DEBUG
    if ( mode != async_block_on_nothing )
       printf( "Async: %s - w %d, d %d, a %d, h %d\n",
               mode == async_block_on_write ? "blk_write" : "blk_read",
               wc, dc, ac, hc );
#endif
}

/*
 * install a handler for DC_APPL packets (can be NULL), returning old one.
 */
DC_Appl_Handler Adp_Install_DC_Appl_Handler(const DC_Appl_Handler handler)
{
    DC_Appl_Handler old_handler = dc_appl_handler;

#ifdef DEBUG
    printf( "Installing DC_APPL handler %x (old %x)\n", handler, old_handler );
#endif

    dc_appl_handler = handler;
    return old_handler;
}


/*
 * add an asynchronous processing callback to the list
 * TRUE == okay, FALSE == no more async processing slots
 */
bool Adp_Install_Async_Callback( const Adp_Async_Callback callback_proc )
{
    if ( num_async_callbacks < MAX_ASYNC_CALLBACKS && callback_proc != NULL )
    {
        async_callbacks[num_async_callbacks] = callback_proc;
        ++num_async_callbacks;
        return TRUE;
    }
    else
       return FALSE;
}


/*
 * delay for a given period (in microseconds)
 */
void Adp_delay(unsigned int period)
{
    struct timeval tv;

#ifdef DEBUG
    printf("delaying for %d microseconds\n", period);
#endif
    tv.tv_sec = (period / 1000000);
    tv.tv_usec = (period % 1000000);

    (void)select(0, NULL, NULL, NULL, &tv);
}

/* EOF hostchan.c */
