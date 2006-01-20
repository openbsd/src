/*	$OpenBSD: adb.c,v 1.20 2006/01/20 00:10:09 miod Exp $	*/
/*	$NetBSD: adb.c,v 1.47 2005/06/16 22:43:36 jmc Exp $	*/
/*	$NetBSD: adb_direct.c,v 1.51 2005/06/16 22:43:36 jmc Exp $	*/

/*
 * Copyright (C) 1996, 1997 John P. Wittkoski
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by John P. Wittkoski.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (C) 1994	Bradley A. Grantham
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bradley A. Grantham.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code is rather messy, but I don't have time right now
 * to clean it up as much as I would like.
 * But it works, so I'm happy. :-) jpw
 */
 
/*
 * TO DO:
 *  - We could reduce the time spent in the adb_intr_* routines
 *    by having them save the incoming and outgoing data directly 
 *    in the adbInbound and adbOutbound queues, as it would reduce
 *    the number of times we need to copy the data around. It
 *    would also make the code more readable and easier to follow.
 *  - (Related to above) Use the header part of adbCommand to 
 *    reduce the number of copies we have to do of the data.
 *  - (Related to above) Actually implement the adbOutbound queue.
 *    This is fairly easy once you switch all the intr routines
 *    over to using adbCommand structs directly.
 *  - There is a bug in the state machine of adb_intr_cuda
 *    code that causes hangs, especially on 030 machines, probably
 *    because of some timing issues. Because I have been unable to 
 *    determine the exact cause of this bug, I used the timeout function 
 *    to check for and recover from this condition. If anyone finds 
 *    the actual cause of this bug, the calls to timeout and the 
 *    adb_cuda_tickle routine can be removed.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/timeout.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/viareg.h>

#include <dev/adb/adb.h>
#include <mac68k/dev/adbvar.h>

#define printf_intr printf

int	adb_polling;		/* Are we polling?  (Debugger mode) */
#ifdef ADB_DEBUG
int	adb_debug;		/* Output debugging messages */
#endif /* ADB_DEBUG */

/* some misc. leftovers */
#define vPB		0x0000
#define vPB3		0x08
#define vPB4		0x10
#define vPB5		0x20
#define vSR_INT		0x04
#define vSR_OUT		0x10

/* the type of ADB action that we are currently preforming */
#define ADB_ACTION_NOTREADY	0x1	/* has not been initialized yet */
#define ADB_ACTION_IDLE		0x2	/* the bus is currently idle */
#define ADB_ACTION_OUT		0x3	/* sending out a command */
#define ADB_ACTION_IN		0x4	/* receiving data */
#define ADB_ACTION_POLLING	0x5	/* polling - II only */

/*
 * These describe the state of the ADB bus itself, although they
 * don't necessarily correspond directly to ADB states.
 * Note: these are not really used in the IIsi code.
 */
#define ADB_BUS_UNKNOWN		0x1	/* we don't know yet - all models */
#define ADB_BUS_IDLE		0x2	/* bus is idle - all models */
#define ADB_BUS_CMD		0x3	/* starting a command - II models */
#define ADB_BUS_ODD		0x4	/* the "odd" state - II models */
#define ADB_BUS_EVEN		0x5	/* the "even" state - II models */
#define ADB_BUS_ACTIVE		0x6	/* active state - IIsi models */
#define ADB_BUS_ACK		0x7	/* currently ACKing - IIsi models */

/*
 * Shortcuts for setting or testing the VIA bit states.
 * Not all shortcuts are used for every type of ADB hardware.
 */
#define ADB_SET_STATE_IDLE_II()		via_reg(VIA1, vBufB) |= (vPB4 | vPB5)
#define ADB_SET_STATE_IDLE_IISI()	via_reg(VIA1, vBufB) &= ~(vPB4 | vPB5)
#define ADB_SET_STATE_IDLE_CUDA()	via_reg(VIA1, vBufB) |= (vPB4 | vPB5)
#define ADB_SET_STATE_CMD()		via_reg(VIA1, vBufB) &= ~(vPB4 | vPB5)
#define ADB_SET_STATE_EVEN()		via_reg(VIA1, vBufB) = ((via_reg(VIA1, \
						vBufB) | vPB4) & ~vPB5)
#define ADB_SET_STATE_ODD()		via_reg(VIA1, vBufB) = ((via_reg(VIA1, \
						vBufB) | vPB5) & ~vPB4)
#define ADB_SET_STATE_ACTIVE() 		via_reg(VIA1, vBufB) |= vPB5
#define ADB_SET_STATE_INACTIVE()	via_reg(VIA1, vBufB) &= ~vPB5
#define ADB_SET_STATE_TIP()		via_reg(VIA1, vBufB) &= ~vPB5
#define ADB_CLR_STATE_TIP() 		via_reg(VIA1, vBufB) |= vPB5
#define ADB_SET_STATE_ACKON()		via_reg(VIA1, vBufB) |= vPB4
#define ADB_SET_STATE_ACKOFF()		via_reg(VIA1, vBufB) &= ~vPB4
#define ADB_TOGGLE_STATE_ACK_CUDA()	via_reg(VIA1, vBufB) ^= vPB4
#define ADB_SET_STATE_ACKON_CUDA()	via_reg(VIA1, vBufB) &= ~vPB4
#define ADB_SET_STATE_ACKOFF_CUDA()	via_reg(VIA1, vBufB) |= vPB4
#define ADB_SET_SR_INPUT()		via_reg(VIA1, vACR) &= ~vSR_OUT
#define ADB_SET_SR_OUTPUT()		via_reg(VIA1, vACR) |= vSR_OUT
#define ADB_SR()			via_reg(VIA1, vSR)
#define ADB_VIA_INTR_ENABLE()		via_reg(VIA1, vIER) = 0x84
#define ADB_VIA_INTR_DISABLE()		via_reg(VIA1, vIER) = 0x04
#define ADB_VIA_CLR_INTR()		via_reg(VIA1, vIFR) = 0x04
#define ADB_INTR_IS_OFF			(vPB3 == (via_reg(VIA1, vBufB) & vPB3))
#define ADB_INTR_IS_ON			(0 == (via_reg(VIA1, vBufB) & vPB3))
#define ADB_SR_INTR_IS_OFF		(0 == (via_reg(VIA1, vIFR) & vSR_INT))
#define ADB_SR_INTR_IS_ON		(vSR_INT == (via_reg(VIA1, \
						vIFR) & vSR_INT))

/*
 * This is the delay that is required (in uS) between certain
 * ADB transactions. The actual timing delay for for each uS is
 * calculated at boot time to account for differences in machine speed.
 */
#define ADB_DELAY	150

/*
 * Maximum ADB message length; includes space for data, result, and
 * device code - plus a little for safety.
 */
#define ADB_MAX_MSG_LENGTH	16
#define ADB_MAX_HDR_LENGTH	8

#define ADB_QUEUE		32
#define ADB_TICKLE_TICKS	4

/*
 * A structure for storing information about each ADB device.
 */
struct ADBDevEntry {
	void	(*ServiceRtPtr)(void);
	void	*DataAreaAddr;
	int	devType;
	int	origAddr;
	int	currentAddr;
};

/*
 * Used to hold ADB commands that are waiting to be sent out.
 */
struct adbCmdHoldEntry {
	u_char	outBuf[ADB_MAX_MSG_LENGTH];	/* our message */
	u_char	*saveBuf;	/* buffer to know where to save result */
	u_char	*compRout;	/* completion routine pointer */
	u_char	*data;		/* completion routine data pointer */
};

/*
 * Eventually used for two separate queues, the queue between 
 * the upper and lower halves, and the outgoing packet queue.
 * TO DO: adbCommand can replace all of adbCmdHoldEntry eventually
 */
struct adbCommand {
	u_char	header[ADB_MAX_HDR_LENGTH];	/* not used yet */
	u_char	data[ADB_MAX_MSG_LENGTH];	/* packet data only */
	u_char	*saveBuf;	/* where to save result */
	u_char	*compRout;	/* completion routine pointer */
	u_char	*compData;	/* completion routine data pointer */
	u_int	cmd;		/* the original command for this data */
	u_int	unsol;		/* 1 if packet was unsolicited */
	u_int	ack_only;	/* 1 for no special processing */
};

/*
 * Text representations of each hardware class
 */
const char	*adbHardwareDescr[MAX_ADB_HW + 1] = {
	"unknown",
	"II series",
	"IIsi series",
	"PowerBook",
	"Cuda",
};

/*
 * A few variables that we need and their initial values.
 */
int	adbHardware = ADB_HW_UNKNOWN;
int	adbActionState = ADB_ACTION_NOTREADY;
int	adbBusState = ADB_BUS_UNKNOWN;
int	adbWaiting = 0;		/* waiting for return data from the device */
int	adbWriteDelay = 0;	/* working on (or waiting to do) a write */
int	adbOutQueueHasData = 0;	/* something in the queue waiting to go out */
int	adbSoftPower = 0;	/* machine supports soft power */

int	adbWaitingCmd = 0;	/* ADB command we are waiting for */
u_char	*adbBuffer = (long)0;	/* pointer to user data area */
void	*adbCompRout = (long)0;	/* pointer to the completion routine */
void	*adbCompData = (long)0;	/* pointer to the completion routine data */
int	adbStarting = 1;	/* doing adb_reinit so do polling differently */

u_char	adbInputBuffer[ADB_MAX_MSG_LENGTH];	/* data input buffer */
u_char	adbOutputBuffer[ADB_MAX_MSG_LENGTH];	/* data output buffer */
struct	adbCmdHoldEntry adbOutQueue;		/* our 1 entry output queue */

int	adbSentChars = 0;	/* how many characters we have sent */
int	adbLastDevice = 0;	/* last ADB dev we heard from (II ONLY) */

struct	ADBDevEntry ADBDevTable[16];	/* our ADB device table */
int	ADBNumDevices;		/* num. of ADB devices found with adb_reinit */

struct	adbCommand adbInbound[ADB_QUEUE];	/* incoming queue */
volatile int	adbInCount = 0;		/* how many packets in in queue */
int	adbInHead = 0;			/* head of in queue */
int	adbInTail = 0;			/* tail of in queue */
struct	adbCommand adbOutbound[ADB_QUEUE]; /* outgoing queue - not used yet */
int	adbOutCount = 0;		/* how many packets in out queue */
int	adbOutHead = 0;			/* head of out queue */
int	adbOutTail = 0;			/* tail of out queue */

int	tickle_count = 0;		/* how many tickles seen for this packet? */
int	tickle_serial = 0;		/* the last packet tickled */
int	adb_cuda_serial = 0;		/* the current packet */

struct timeout adb_cuda_timeout;

void	pm_setup_adb(void);
void	pm_hw_setup(void);
void	pm_check_adb_devices(int);
int	pm_adb_op(u_char *, void *, void *, int);
void	pm_init_adb_device(void);

/*
 * The following are private routines.
 */
#ifdef ADB_DEBUG
void	print_single(u_char *);
#endif
int	adb_intr(void *);
int	adb_intr_II(void *);
int	adb_intr_IIsi(void *);
int	adb_intr_cuda(void *);
void	adb_soft_intr(void);
int	send_adb_II(u_char *, u_char *, void *, void *, int);
int	send_adb_IIsi(u_char *, u_char *, void *, void *, int);
int	send_adb_cuda(u_char *, u_char *, void *, void *, int);
void	adb_intr_cuda_test(void);
void	adb_cuda_tickle(void);
void	adb_pass_up(struct adbCommand *);
void	adb_op_comprout(caddr_t, caddr_t, int);
void	adb_reinit(void);
int	count_adbs(void);
int	get_ind_adb_info(ADBDataBlock *, int);
int	get_adb_info(ADBDataBlock *, int);
int	set_adb_info(ADBSetInfoBlock *, int);
void	adb_setup_hw_type(void);
int	adb_op(Ptr, Ptr, Ptr, short);
void	adb_read_II(u_char *);
void	adb_hw_setup(void);
void	adb_hw_setup_IIsi(u_char *);
int	adb_cmd_result(u_char *);
int	adb_guess_next_device(void);
int	adb_prog_switch_enable(void);
int	adb_prog_switch_disable(void);
/* we should create this and it will be the public version */
int	send_adb(u_char *, void *, void *);

#ifdef ADB_DEBUG
/*
 * print_single
 * Diagnostic display routine. Displays the hex values of the
 * specified elements of the u_char. The length of the "string"
 * is in [0].
 */
void
print_single(u_char *str)
{
	int x;

	if (str == 0) {
		printf_intr("no data - null pointer\n");
		return;
	}
	if (*str == 0) {
		printf_intr("nothing returned\n");
		return;
	}
	if (*str > 20) {
		printf_intr("ADB: ACK > 20 no way!\n");
		*str = (u_char)20;
	}
	printf_intr("(length=0x%x):", (u_int)*str);
	for (x = 1; x <= *str; x++)
		printf_intr("  0x%02x", (u_int)*(str + x));
	printf_intr("\n");
}
#endif

void
adb_cuda_tickle(void)
{
	volatile int s;

	if (adbActionState == ADB_ACTION_IN) {
		if (tickle_serial == adb_cuda_serial) {
			if (++tickle_count > 0) {
				s = splhigh();
				adbActionState = ADB_ACTION_IDLE;
				adbInputBuffer[0] = 0;
				ADB_SET_STATE_IDLE_CUDA();
				splx(s);
			}
		} else {
			tickle_serial = adb_cuda_serial;
			tickle_count = 0;
		}
	} else {
		tickle_serial = adb_cuda_serial;
		tickle_count = 0;
	}

	timeout_add(&adb_cuda_timeout, ADB_TICKLE_TICKS);
}

/*
 * called when when an adb interrupt happens
 *
 * Cuda version of adb_intr
 * TO DO: do we want to add some calls to intr_dispatch() here to
 * grab serial interrupts?
 */
int
adb_intr_cuda(void *arg)
{
	volatile int i, ending;
	volatile unsigned int s;
	struct adbCommand packet;

	s = splhigh();		/* can't be too careful - might be called */
	/* from a routine, NOT an interrupt */

	ADB_VIA_CLR_INTR();	/* clear interrupt */
	ADB_VIA_INTR_DISABLE();	/* disable ADB interrupt on IIs. */

switch_start:
	switch (adbActionState) {
	case ADB_ACTION_IDLE:
		/*
		 * This is an unexpected packet, so grab the first (dummy)
		 * byte, set up the proper vars, and tell the chip we are
		 * starting to receive the packet by setting the TIP bit.
		 */
		adbInputBuffer[1] = ADB_SR();
		adb_cuda_serial++;
		if (ADB_INTR_IS_OFF)	/* must have been a fake start */
			break;

		ADB_SET_SR_INPUT();
		ADB_SET_STATE_TIP();

		adbInputBuffer[0] = 1;
		adbActionState = ADB_ACTION_IN;
#ifdef ADB_DEBUG
		if (adb_debug)
			printf_intr("idle 0x%02x ", adbInputBuffer[1]);
#endif
		break;

	case ADB_ACTION_IN:
		adbInputBuffer[++adbInputBuffer[0]] = ADB_SR();
		/* intr off means this is the last byte (end of frame) */
		if (ADB_INTR_IS_OFF)
			ending = 1;
		else
			ending = 0;

		if (1 == ending) {	/* end of message? */
#ifdef ADB_DEBUG
			if (adb_debug) {
				printf_intr("in end 0x%02x ",
				    adbInputBuffer[adbInputBuffer[0]]);
				print_single(adbInputBuffer);
			}
#endif

			/*
			 * Are we waiting AND does this packet match what we
			 * are waiting for AND is it coming from either the
			 * ADB or RTC/PRAM sub-device? This section _should_
			 * recognize all ADB and RTC/PRAM type commands, but
			 * there may be more... NOTE: commands are always at
			 * [4], even for RTC/PRAM commands.
			 */
			/* set up data for adb_pass_up */
			memcpy(packet.data, adbInputBuffer, adbInputBuffer[0] + 1);
				
			if ((adbWaiting == 1) &&
			    (adbInputBuffer[4] == adbWaitingCmd) &&
			    ((adbInputBuffer[2] == 0x00) ||
			    (adbInputBuffer[2] == 0x01))) {
				packet.saveBuf = adbBuffer;
				packet.compRout = adbCompRout;
				packet.compData = adbCompData;
				packet.unsol = 0;
				packet.ack_only = 0;
				adb_pass_up(&packet);

				adbWaitingCmd = 0;	/* reset "waiting" vars */
				adbWaiting = 0;
				adbBuffer = (long)0;
				adbCompRout = (long)0;
				adbCompData = (long)0;
			} else {
				packet.unsol = 1;
				packet.ack_only = 0;
				adb_pass_up(&packet);
			}


			/* reset vars and signal the end of this frame */
			adbActionState = ADB_ACTION_IDLE;
			adbInputBuffer[0] = 0;
			ADB_SET_STATE_IDLE_CUDA();
			/*ADB_SET_SR_INPUT();*/

			/*
			 * If there is something waiting to be sent out,
			 * the set everything up and send the first byte.
			 */
			if (adbWriteDelay == 1) {
				delay(ADB_DELAY);	/* required */
				adbSentChars = 0;
				adbActionState = ADB_ACTION_OUT;
				/*
				 * If the interrupt is on, we were too slow
				 * and the chip has already started to send
				 * something to us, so back out of the write
				 * and start a read cycle.
				 */
				if (ADB_INTR_IS_ON) {
					ADB_SET_SR_INPUT();
					ADB_SET_STATE_IDLE_CUDA();
					adbSentChars = 0;
					adbActionState = ADB_ACTION_IDLE;
					adbInputBuffer[0] = 0;
					break;
				}
				/*
				 * If we got here, it's ok to start sending
				 * so load the first byte and tell the chip
				 * we want to send.
				 */
				ADB_SET_STATE_TIP();
				ADB_SET_SR_OUTPUT();
				ADB_SR() = adbOutputBuffer[adbSentChars + 1];
			}
		} else {
			ADB_TOGGLE_STATE_ACK_CUDA();
#ifdef ADB_DEBUG
			if (adb_debug)
				printf_intr("in 0x%02x ",
				    adbInputBuffer[adbInputBuffer[0]]);
#endif
		}
		break;

	case ADB_ACTION_OUT:
		i = ADB_SR();	/* reset SR-intr in IFR */
#ifdef ADB_DEBUG
		if (adb_debug)
			printf_intr("intr out 0x%02x ", i);
#endif

		adbSentChars++;
		if (ADB_INTR_IS_ON) {	/* ADB intr low during write */
#ifdef ADB_DEBUG
			if (adb_debug)
				printf_intr("intr was on ");
#endif
			ADB_SET_SR_INPUT();	/* make sure SR is set to IN */
			ADB_SET_STATE_IDLE_CUDA();
			adbSentChars = 0;	/* must start all over */
			adbActionState = ADB_ACTION_IDLE;	/* new state */
			adbInputBuffer[0] = 0;
			adbWriteDelay = 1;	/* must retry when done with
						 * read */
			delay(ADB_DELAY);
			goto switch_start;	/* process next state right
						 * now */
			break;
		}
		if (adbOutputBuffer[0] == adbSentChars) {	/* check for done */
			if (0 == adb_cmd_result(adbOutputBuffer)) {	/* do we expect data
									 * back? */
				adbWaiting = 1;	/* signal waiting for return */
				adbWaitingCmd = adbOutputBuffer[2];	/* save waiting command */
			} else {	/* no talk, so done */
				/* set up stuff for adb_pass_up */
				memcpy(packet.data, adbInputBuffer, adbInputBuffer[0] + 1);
				packet.saveBuf = adbBuffer;
				packet.compRout = adbCompRout;
				packet.compData = adbCompData;
				packet.cmd = adbWaitingCmd;
				packet.unsol = 0;
				packet.ack_only = 1;
				adb_pass_up(&packet);

				/* reset "waiting" vars, just in case */
				adbWaitingCmd = 0;
				adbBuffer = (long)0;
				adbCompRout = (long)0;
				adbCompData = (long)0;
			}

			adbWriteDelay = 0;	/* done writing */
			adbActionState = ADB_ACTION_IDLE;	/* signal bus is idle */
			ADB_SET_SR_INPUT();
			ADB_SET_STATE_IDLE_CUDA();
#ifdef ADB_DEBUG
			if (adb_debug)
				printf_intr("write done ");
#endif
		} else {
			ADB_SR() = adbOutputBuffer[adbSentChars + 1];	/* send next byte */
			ADB_TOGGLE_STATE_ACK_CUDA();	/* signal byte ready to
							 * shift */
#ifdef ADB_DEBUG
			if (adb_debug)
				printf_intr("toggle ");
#endif
		}
		break;

	case ADB_ACTION_NOTREADY:
#ifdef ADB_DEBUG
		if (adb_debug)
			printf_intr("adb: not yet initialized\n");
#endif
		break;

	default:
#ifdef ADB_DEBUG
		if (adb_debug)
			printf_intr("intr: unknown ADB state\n");
#endif
		break;
	}

	ADB_VIA_INTR_ENABLE();	/* enable ADB interrupt on IIs. */

	splx(s);		/* restore */

	return (1);
}				/* end adb_intr_cuda */


int
send_adb_cuda(u_char *in, u_char *buffer, void *compRout, void *data, int
	command)
{
	int s, len;

#ifdef ADB_DEBUG
	if (adb_debug)
		printf_intr("SEND\n");
#endif

	if (adbActionState == ADB_ACTION_NOTREADY)
		return 1;

	/* Don't interrupt while we are messing with the ADB */
	s = splhigh();

	if ((adbActionState == ADB_ACTION_IDLE) &&	/* ADB available? */
	    (ADB_INTR_IS_OFF)) {	/* and no incoming interrupt? */
	} else
		if (adbWriteDelay == 0)	/* it's busy, but is anything waiting? */
			adbWriteDelay = 1;	/* if no, then we'll "queue"
						 * it up */
		else {
			splx(s);
			return 1;	/* really busy! */
		}

#ifdef ADB_DEBUG
	if (adb_debug)
		printf_intr("QUEUE\n");
#endif
	if ((long)in == (long)0) {	/* need to convert? */
		/*
		 * Don't need to use adb_cmd_extra here because this section
		 * will be called ONLY when it is an ADB command (no RTC or
		 * PRAM)
		 */
		if ((command & 0x0c) == 0x08)	/* copy addl data ONLY if
						 * doing a listen! */
			len = buffer[0];	/* length of additional data */
		else
			len = 0;/* no additional data */

		adbOutputBuffer[0] = 2 + len;	/* dev. type + command + addl.
						 * data */
		adbOutputBuffer[1] = 0x00;	/* mark as an ADB command */
		adbOutputBuffer[2] = (u_char)command;	/* load command */

		/* copy additional output data, if any */
		memcpy(adbOutputBuffer + 3, buffer + 1, len);
	} else
		/* if data ready, just copy over */
		memcpy(adbOutputBuffer, in, in[0] + 2);

	adbSentChars = 0;	/* nothing sent yet */
	adbBuffer = buffer;	/* save buffer to know where to save result */
	adbCompRout = compRout;	/* save completion routine pointer */
	adbCompData = data;	/* save completion routine data pointer */
	adbWaitingCmd = adbOutputBuffer[2];	/* save wait command */

	if (adbWriteDelay != 1) {	/* start command now? */
#ifdef ADB_DEBUG
		if (adb_debug)
			printf_intr("out start NOW");
#endif
		delay(ADB_DELAY);
		adbActionState = ADB_ACTION_OUT;	/* set next state */
		ADB_SET_SR_OUTPUT();	/* set shift register for OUT */
		ADB_SR() = adbOutputBuffer[adbSentChars + 1];	/* load byte for output */
		ADB_SET_STATE_ACKOFF_CUDA();
		ADB_SET_STATE_TIP();	/* tell ADB that we want to send */
	}
	adbWriteDelay = 1;	/* something in the write "queue" */

	splx(s);

	if (0x0100 <= (s & 0x0700))	/* were VIA1 interrupts blocked? */
		/* poll until byte done */
		while ((adbActionState != ADB_ACTION_IDLE) || (ADB_INTR_IS_ON)
		    || (adbWaiting == 1))
			if (ADB_SR_INTR_IS_ON) { /* wait for "interrupt" */
				adb_intr_cuda(NULL); /* go process it */
				if (adb_polling)
					adb_soft_intr();
			}

	return 0;
}				/* send_adb_cuda */


int
adb_intr_II(void *arg)
{
	struct adbCommand packet;
	int i, intr_on = 0;
	int send = 0;
	unsigned int s;

	s = splhigh();		/* can't be too careful - might be called */
	/* from a routine, NOT an interrupt */

	ADB_VIA_CLR_INTR();	/* clear interrupt */

	ADB_VIA_INTR_DISABLE();	/* disable ADB interrupt on IIs. */

	delay(ADB_DELAY);	/* yuck (don't remove) */

	(void)intr_dispatch(0x70); /* grab any serial interrupts */

	if (ADB_INTR_IS_ON)
		intr_on = 1;	/* save for later */
	
switch_start:
	switch (adbActionState) {
	case ADB_ACTION_POLLING:
		if (!intr_on) {
			if (adbOutQueueHasData) {
#ifdef ADB_DEBUG
				if (adb_debug & 0x80)
					printf_intr("POLL-doing-out-queue. ");
#endif
				ADB_SET_STATE_IDLE_II();
				delay(ADB_DELAY);

				/* copy over data */
				memcpy(adbOutputBuffer, adbOutQueue.outBuf,
				    adbOutQueue.outBuf[0] + 2);

				adbBuffer = adbOutQueue.saveBuf;	/* user data area */
				adbCompRout = adbOutQueue.compRout;	/* completion routine */
				adbCompData = adbOutQueue.data;	/* comp. rout. data */
				adbOutQueueHasData = 0;	/* currently processing
							 * "queue" entry */
				adbSentChars = 0;	/* nothing sent yet */
				adbActionState = ADB_ACTION_OUT;	/* set next state */
				ADB_SET_SR_OUTPUT();	/* set shift register for OUT */
				ADB_SR() = adbOutputBuffer[1];	/* load byte for output */
				adbBusState = ADB_BUS_CMD;	/* set bus to cmd state */
				ADB_SET_STATE_CMD();	/* tell ADB that we want to send */
				break;
			} else {
#ifdef ADB_DEBUG
				if (adb_debug)
					printf_intr("pIDLE ");
#endif
				adbActionState = ADB_ACTION_IDLE;
			}
		} else {
#ifdef ADB_DEBUG
			if (adb_debug & 0x80)
				printf_intr("pIN ");
#endif
			adbActionState = ADB_ACTION_IN;
		}
		delay(ADB_DELAY);
		(void)intr_dispatch(0x70); /* grab any serial interrupts */
		goto switch_start;
		break;
	case ADB_ACTION_IDLE:
		if (!intr_on) {
			i = ADB_SR();
			adbBusState = ADB_BUS_IDLE;
			adbActionState = ADB_ACTION_IDLE;
			ADB_SET_STATE_IDLE_II();
			break;
		}
		adbInputBuffer[0] = 1;
		adbInputBuffer[1] = ADB_SR();	/* get first byte */
#ifdef ADB_DEBUG
		if (adb_debug & 0x80)
			printf_intr("idle 0x%02x ", adbInputBuffer[1]);
#endif
		ADB_SET_SR_INPUT();	/* make sure SR is set to IN */
		adbActionState = ADB_ACTION_IN;	/* set next state */
		ADB_SET_STATE_EVEN();	/* set bus state to even */
		adbBusState = ADB_BUS_EVEN;
		break;

	case ADB_ACTION_IN:
		adbInputBuffer[++adbInputBuffer[0]] = ADB_SR();	/* get byte */
#ifdef ADB_DEBUG
		if (adb_debug & 0x80)
			printf_intr("in 0x%02x ",
			    adbInputBuffer[adbInputBuffer[0]]);
#endif
		ADB_SET_SR_INPUT();	/* make sure SR is set to IN */

		if (intr_on) {	/* process last byte of packet */
			adbInputBuffer[0]--;	/* minus one */
			/*
			 * If intr_on was true, and it's the second byte, then
			 * the byte we just discarded is really valid, so
			 * adjust the count
			 */
			if (adbInputBuffer[0] == 2) {
				adbInputBuffer[0]++;
			}
				
#ifdef ADB_DEBUG
			if (adb_debug & 0x80) {
				printf_intr("done: ");
				print_single(adbInputBuffer);
			}
#endif

			adbLastDevice = ADB_CMDADDR(adbInputBuffer[1]);
			
			if (adbInputBuffer[0] == 1 && !adbWaiting) {	/* SRQ!!!*/
#ifdef ADB_DEBUG
				if (adb_debug & 0x80)
					printf_intr(" xSRQ! ");
#endif
				adb_guess_next_device();
#ifdef ADB_DEBUG
				if (adb_debug & 0x80)
					printf_intr("try 0x%0x ",
					    adbLastDevice);
#endif
				adbOutputBuffer[0] = 1;
				adbOutputBuffer[1] = ADBTALK(adbLastDevice, 0);
			
				adbSentChars = 0;	/* nothing sent yet */
				adbActionState = ADB_ACTION_POLLING;	/* set next state */
				ADB_SET_SR_OUTPUT();	/* set shift register for OUT */
				ADB_SR() = adbOutputBuffer[1];	/* load byte for output */
				adbBusState = ADB_BUS_CMD;	/* set bus to cmd state */
				ADB_SET_STATE_CMD();	/* tell ADB that we want to */
				break;
			}
				
			/* set up data for adb_pass_up */
			memcpy(packet.data, adbInputBuffer, adbInputBuffer[0] + 1);

			if (!adbWaiting && (adbInputBuffer[0] != 0)) {
				packet.unsol = 1;
				packet.ack_only = 0;
				adb_pass_up(&packet);
			} else {
				packet.saveBuf = adbBuffer;
				packet.compRout = adbCompRout;
				packet.compData = adbCompData;
				packet.unsol = 0;
				packet.ack_only = 0;
				adb_pass_up(&packet);
			}

			adbWaiting = 0;
			adbInputBuffer[0] = 0;
			adbBuffer = (long)0;
			adbCompRout = (long)0;
			adbCompData = (long)0;
			/*
			 * Since we are done, check whether there is any data
			 * waiting to do out. If so, start the sending the data.
			 */
			if (adbOutQueueHasData == 1) {
#ifdef ADB_DEBUG
				if (adb_debug & 0x80)
					printf_intr("XXX: DOING OUT QUEUE\n");
#endif
				/* copy over data */
				memcpy(adbOutputBuffer, adbOutQueue.outBuf,
				    adbOutQueue.outBuf[0] + 2);
				adbBuffer = adbOutQueue.saveBuf;	/* user data area */
				adbCompRout = adbOutQueue.compRout;	/* completion routine */
				adbCompData = adbOutQueue.data;	/* comp. rout. data */
				adbOutQueueHasData = 0;	/* currently processing
							 * "queue" entry */
				send = 1;
			} else {
#ifdef ADB_DEBUG
				if (adb_debug & 0x80)
					printf_intr("XXending ");
#endif
				adb_guess_next_device();
				adbOutputBuffer[0] = 1;
				adbOutputBuffer[1] = ((adbLastDevice & 0x0f) << 4) | 0x0c;
				adbSentChars = 0;	/* nothing sent yet */
				adbActionState = ADB_ACTION_POLLING;	/* set next state */
				ADB_SET_SR_OUTPUT();	/* set shift register for OUT */
				ADB_SR() = adbOutputBuffer[1];	/* load byte for output */
				adbBusState = ADB_BUS_CMD;	/* set bus to cmd state */
				ADB_SET_STATE_CMD();	/* tell ADB that we want to */
				break;
			}
		}

		/*
		 * If send is true then something above determined that
		 * the message has ended and we need to start sending out
		 * a new message immediately. This could be because there
		 * is data waiting to go out or because an SRQ was seen.
		 */
		if (send) {
			adbSentChars = 0;	/* nothing sent yet */
			adbActionState = ADB_ACTION_OUT;	/* set next state */
			ADB_SET_SR_OUTPUT();	/* set shift register for OUT */
			ADB_SR() = adbOutputBuffer[1];	/* load byte for output */
			adbBusState = ADB_BUS_CMD;	/* set bus to cmd state */
			ADB_SET_STATE_CMD();	/* tell ADB that we want to
						 * send */
			break;
		}
		/* We only get this far if the message hasn't ended yet. */
		switch (adbBusState) {	/* set to next state */
		case ADB_BUS_EVEN:
			ADB_SET_STATE_ODD();	/* set state to odd */
			adbBusState = ADB_BUS_ODD;
			break;

		case ADB_BUS_ODD:
			ADB_SET_STATE_EVEN();	/* set state to even */
			adbBusState = ADB_BUS_EVEN;
			break;
		default:
			printf_intr("strange state!!!\n");	/* huh? */
			break;
		}
		break;

	case ADB_ACTION_OUT:
		i = ADB_SR();	/* clear interrupt */
		adbSentChars++;
		/*
		 * If the outgoing data was a TALK, we must
		 * switch to input mode to get the result.
		 */
		if ((adbOutputBuffer[1] & 0x0c) == 0x0c) {
			adbInputBuffer[0] = 1;
			adbInputBuffer[1] = i;
			adbActionState = ADB_ACTION_IN;
			ADB_SET_SR_INPUT();
			adbBusState = ADB_BUS_EVEN;
			ADB_SET_STATE_EVEN();
#ifdef ADB_DEBUG
			if (adb_debug & 0x80)
				printf_intr("talk out 0x%02x ", i);
#endif
			/* we want something back */
			adbWaiting = 1;
			break;
		}
		/*
		 * If it's not a TALK, check whether all data has been sent.
		 * If so, call the completion routine and clean up. If not,
		 * advance to the next state.
		 */
#ifdef ADB_DEBUG
		if (adb_debug & 0x80)
			printf_intr("non-talk out 0x%0x ", i);
#endif
		ADB_SET_SR_OUTPUT();
		if (adbOutputBuffer[0] == adbSentChars) {	/* check for done */
#ifdef ADB_DEBUG
			if (adb_debug & 0x80)
				printf_intr("done \n");
#endif
			/* set up stuff for adb_pass_up */
			memcpy(packet.data, adbOutputBuffer, adbOutputBuffer[0] + 1);
			packet.saveBuf = adbBuffer;
			packet.compRout = adbCompRout;
			packet.compData = adbCompData;
			packet.cmd = adbWaitingCmd;
			packet.unsol = 0;
			packet.ack_only = 1;
			adb_pass_up(&packet);

			/* reset "waiting" vars, just in case */ 
			adbBuffer = (long)0;
			adbCompRout = (long)0;
			adbCompData = (long)0;
			if (adbOutQueueHasData == 1) {
				/* copy over data */
				memcpy(adbOutputBuffer, adbOutQueue.outBuf,
				    adbOutQueue.outBuf[0] + 2);
				adbBuffer = adbOutQueue.saveBuf;	/* user data area */
				adbCompRout = adbOutQueue.compRout;	/* completion routine */
				adbCompData = adbOutQueue.data;	/* comp. rout. data */
				adbOutQueueHasData = 0;	/* currently processing
							 * "queue" entry */
				adbSentChars = 0;	/* nothing sent yet */
				adbActionState = ADB_ACTION_OUT;	/* set next state */
				ADB_SET_SR_OUTPUT();	/* set shift register for OUT */
				ADB_SR() = adbOutputBuffer[1];	/* load byte for output */
				adbBusState = ADB_BUS_CMD;	/* set bus to cmd state */
				ADB_SET_STATE_CMD();	/* tell ADB that we want to
							 * send */
				break;
			} else {
				/* send talk to last device instead */
				adbOutputBuffer[0] = 1;
				adbOutputBuffer[1] =
				    ADBTALK(ADB_CMDADDR(adbOutputBuffer[1]), 0);
			
				adbSentChars = 0;	/* nothing sent yet */
				adbActionState = ADB_ACTION_IDLE;	/* set next state */
				ADB_SET_SR_OUTPUT();	/* set shift register for OUT */
				ADB_SR() = adbOutputBuffer[1];	/* load byte for output */
				adbBusState = ADB_BUS_CMD;	/* set bus to cmd state */
				ADB_SET_STATE_CMD();	/* tell ADB that we want to */
				break;
			}
		}
		ADB_SR() = adbOutputBuffer[adbSentChars + 1];
		switch (adbBusState) {	/* advance to next state */
		case ADB_BUS_EVEN:
			ADB_SET_STATE_ODD();	/* set state to odd */
			adbBusState = ADB_BUS_ODD;
			break;

		case ADB_BUS_CMD:
		case ADB_BUS_ODD:
			ADB_SET_STATE_EVEN();	/* set state to even */
			adbBusState = ADB_BUS_EVEN;
			break;

		default:
#ifdef ADB_DEBUG
			if (adb_debug) {
				printf_intr("strange state!!! (0x%x)\n",
				    adbBusState);
			}
#endif
			break;
		}
		break;

	default:
#ifdef ADB_DEBUG
		if (adb_debug)
			printf_intr("adb: unknown ADB state (during intr)\n");
#endif
		break;
	}

	ADB_VIA_INTR_ENABLE();	/* enable ADB interrupt on IIs. */

	splx(s);		/* restore */

	return (1);

}


/*
 * send_adb version for II series machines
 */
int
send_adb_II(u_char *in, u_char *buffer, void *compRout, void *data, int command)
{
	int s, len;

	if (adbActionState == ADB_ACTION_NOTREADY)	/* return if ADB not
							 * available */
		return 1;

	/* Don't interrupt while we are messing with the ADB */
	s = splhigh();

	if (0 != adbOutQueueHasData) {	/* right now, "has data" means "full" */
		splx(s);	/* sorry, try again later */
		return 1;
	}
	if ((long)in == (long)0) {	/* need to convert? */
		/*
		 * Don't need to use adb_cmd_extra here because this section
		 * will be called ONLY when it is an ADB command (no RTC or
		 * PRAM), especially on II series!
		 */
		if ((command & 0x0c) == 0x08)	/* copy addl data ONLY if
						 * doing a listen! */
			len = buffer[0];	/* length of additional data */
		else
			len = 0;/* no additional data */

		adbOutQueue.outBuf[0] = 1 + len;	/* command + addl. data */
		adbOutQueue.outBuf[1] = (u_char)command;	/* load command */

		/* copy additional output data, if any */
		memcpy(adbOutQueue.outBuf + 2, buffer + 1, len);
	} else
		/* if data ready, just copy over */
		memcpy(adbOutQueue.outBuf, in, in[0] + 2);

	adbOutQueue.saveBuf = buffer;	/* save buffer to know where to save
					 * result */
	adbOutQueue.compRout = compRout;	/* save completion routine
						 * pointer */
	adbOutQueue.data = data;/* save completion routine data pointer */

	if ((adbActionState == ADB_ACTION_IDLE) &&	/* is ADB available? */
	    (ADB_INTR_IS_OFF)) {	/* and no incoming interrupts? */
		/* then start command now */
		memcpy(adbOutputBuffer, adbOutQueue.outBuf,
		    adbOutQueue.outBuf[0] + 2);		/* copy over data */

		adbBuffer = adbOutQueue.saveBuf;	/* pointer to user data
							 * area */
		adbCompRout = adbOutQueue.compRout;	/* pointer to the
							 * completion routine */
		adbCompData = adbOutQueue.data;	/* pointer to the completion
						 * routine data */

		adbSentChars = 0;	/* nothing sent yet */
		adbActionState = ADB_ACTION_OUT;	/* set next state */
		adbBusState = ADB_BUS_CMD;	/* set bus to cmd state */

		ADB_SET_SR_OUTPUT();	/* set shift register for OUT */

		ADB_SR() = adbOutputBuffer[adbSentChars + 1];	/* load byte for output */
		ADB_SET_STATE_CMD();	/* tell ADB that we want to send */
		adbOutQueueHasData = 0;	/* currently processing "queue" entry */
	} else
		adbOutQueueHasData = 1;	/* something in the write "queue" */

	splx(s);

	if (0x0100 <= (s & 0x0700))	/* were VIA1 interrupts blocked? */
		/* poll until message done */
		while ((adbActionState != ADB_ACTION_IDLE) || (ADB_INTR_IS_ON)
		    || (adbWaiting == 1))
			if (ADB_SR_INTR_IS_ON) { /* wait for "interrupt" */
				adb_intr_II(NULL); /* go process it */
				if (adb_polling)
					adb_soft_intr();
			}

	return 0;
}


/*
 * This routine is called from the II series interrupt routine
 * to determine what the "next" device is that should be polled.
 */
int
adb_guess_next_device(void)
{
	int last, i, dummy;

	if (adbStarting) {
		/*
		 * Start polling EVERY device, since we can't be sure there is
		 * anything in the device table yet
		 */
		if (adbLastDevice < 1 || adbLastDevice > 15)
			adbLastDevice = 1;
		if (++adbLastDevice > 15)	/* point to next one */
			adbLastDevice = 1;
	} else {
		/* find the next device using the device table */
		if (adbLastDevice < 1 || adbLastDevice > 15)	/* let's be parinoid */
			adbLastDevice = 2;
		last = 1;	/* default index location */

		for (i = 1; i < 16; i++)	/* find index entry */
			if (ADBDevTable[i].currentAddr == adbLastDevice) {	/* look for device */
				last = i;	/* found it */
				break;
			}
		dummy = last;	/* index to start at */
		for (;;) {	/* find next device in index */
			if (++dummy > 15)	/* wrap around if needed */
				dummy = 1;
			if (dummy == last) {	/* didn't find any other
						 * device! This can happen if
						 * there are no devices on the
						 * bus */
				dummy = 1;
				break;
			}
			/* found the next device */
			if (ADBDevTable[dummy].devType != 0)
				break;
		}
		adbLastDevice = ADBDevTable[dummy].currentAddr;
	}
	return adbLastDevice;
}


/*
 * Called when when an adb interrupt happens.
 * This routine simply transfers control over to the appropriate
 * code for the machine we are running on.
 */
int
adb_intr(void *arg)
{
	switch (adbHardware) {
	case ADB_HW_II:
		return adb_intr_II(arg);
		break;

	case ADB_HW_IISI:
		return adb_intr_IIsi(arg);
		break;

	case ADB_HW_PB:		/* Should not come through here. */
		break;

	case ADB_HW_CUDA:
		return adb_intr_cuda(arg);
		break;

	case ADB_HW_UNKNOWN:
		break;
	}

	return (-1);
}


/*
 * called when when an adb interrupt happens
 *
 * IIsi version of adb_intr
 *
 */
int
adb_intr_IIsi(void *arg)
{
	struct adbCommand packet;
	int i, ending;
	unsigned int s;

	s = splhigh();		/* can't be too careful - might be called */
	/* from a routine, NOT an interrupt */

	ADB_VIA_CLR_INTR();	/* clear interrupt */

	ADB_VIA_INTR_DISABLE();	/* disable ADB interrupt on IIs. */

switch_start:
	switch (adbActionState) {
	case ADB_ACTION_IDLE:
		delay(ADB_DELAY);	/* short delay is required before the
					 * first byte */

		ADB_SET_SR_INPUT();	/* make sure SR is set to IN */
		ADB_SET_STATE_ACTIVE();	/* signal start of data frame */
		adbInputBuffer[1] = ADB_SR();	/* get byte */
		adbInputBuffer[0] = 1;
		adbActionState = ADB_ACTION_IN;	/* set next state */

		ADB_SET_STATE_ACKON();	/* start ACK to ADB chip */
		delay(ADB_DELAY);	/* delay */
		ADB_SET_STATE_ACKOFF();	/* end ACK to ADB chip */
		(void)intr_dispatch(0x70); /* grab any serial interrupts */
		break;

	case ADB_ACTION_IN:
		ADB_SET_SR_INPUT();	/* make sure SR is set to IN */
		adbInputBuffer[++adbInputBuffer[0]] = ADB_SR();	/* get byte */
		if (ADB_INTR_IS_OFF)	/* check for end of frame */
			ending = 1;
		else
			ending = 0;

		ADB_SET_STATE_ACKON();	/* start ACK to ADB chip */
		delay(ADB_DELAY);	/* delay */
		ADB_SET_STATE_ACKOFF();	/* end ACK to ADB chip */
		(void)intr_dispatch(0x70); /* grab any serial interrupts */

		if (1 == ending) {	/* end of message? */
			ADB_SET_STATE_INACTIVE();	/* signal end of frame */
			/*
			 * This section _should_ handle all ADB and RTC/PRAM
			 * type commands, but there may be more...  Note:
			 * commands are always at [4], even for rtc/pram
			 * commands
			 */
			/* set up data for adb_pass_up */
			memcpy(packet.data, adbInputBuffer, adbInputBuffer[0] + 1);
				
			if ((adbWaiting == 1) &&	/* are we waiting AND */
			    (adbInputBuffer[4] == adbWaitingCmd) &&	/* the cmd we sent AND */
			    ((adbInputBuffer[2] == 0x00) ||	/* it's from the ADB
								 * device OR */
				(adbInputBuffer[2] == 0x01))) {	/* it's from the
								 * PRAM/RTC device */

				packet.saveBuf = adbBuffer;
				packet.compRout = adbCompRout;
				packet.compData = adbCompData;
				packet.unsol = 0;
				packet.ack_only = 0;
				adb_pass_up(&packet);

				adbWaitingCmd = 0;	/* reset "waiting" vars */
				adbWaiting = 0;
				adbBuffer = (long)0;
				adbCompRout = (long)0;
				adbCompData = (long)0;
			} else {
				packet.unsol = 1;
				packet.ack_only = 0;
				adb_pass_up(&packet);
			}

			adbActionState = ADB_ACTION_IDLE;
			adbInputBuffer[0] = 0;	/* reset length */

			if (adbWriteDelay == 1) {	/* were we waiting to
							 * write? */
				adbSentChars = 0;	/* nothing sent yet */
				adbActionState = ADB_ACTION_OUT;	/* set next state */

				delay(ADB_DELAY);	/* delay */
				(void)intr_dispatch(0x70); /* grab any serial interrupts */

				if (ADB_INTR_IS_ON) {	/* ADB intr low during
							 * write */
					ADB_SET_STATE_IDLE_IISI();	/* reset */
					ADB_SET_SR_INPUT();	/* make sure SR is set
								 * to IN */
					adbSentChars = 0;	/* must start all over */
					adbActionState = ADB_ACTION_IDLE;	/* new state */
					adbInputBuffer[0] = 0;
					/* may be able to take this out later */
					delay(ADB_DELAY);	/* delay */
					break;
				}
				ADB_SET_STATE_ACTIVE();	/* tell ADB that we want
							 * to send */
				ADB_SET_STATE_ACKOFF();	/* make sure */
				ADB_SET_SR_OUTPUT();	/* set shift register
							 * for OUT */
				ADB_SR() = adbOutputBuffer[adbSentChars + 1];
				ADB_SET_STATE_ACKON();	/* tell ADB byte ready
							 * to shift */
			}
		}
		break;

	case ADB_ACTION_OUT:
		i = ADB_SR();	/* reset SR-intr in IFR */
		ADB_SET_SR_OUTPUT();	/* set shift register for OUT */

		ADB_SET_STATE_ACKOFF();	/* finish ACK */
		adbSentChars++;
		if (ADB_INTR_IS_ON) {	/* ADB intr low during write */
			ADB_SET_STATE_IDLE_IISI();	/* reset */
			ADB_SET_SR_INPUT();	/* make sure SR is set to IN */
			adbSentChars = 0;	/* must start all over */
			adbActionState = ADB_ACTION_IDLE;	/* new state */
			adbInputBuffer[0] = 0;
			adbWriteDelay = 1;	/* must retry when done with
						 * read */
			delay(ADB_DELAY);	/* delay */
			(void)intr_dispatch(0x70); /* grab any serial interrupts */
			goto switch_start;	/* process next state right
						 * now */
			break;
		}
		delay(ADB_DELAY);	/* required delay */
		(void)intr_dispatch(0x70); /* grab any serial interrupts */

		if (adbOutputBuffer[0] == adbSentChars) {	/* check for done */
			if (0 == adb_cmd_result(adbOutputBuffer)) {	/* do we expect data
									 * back? */
				adbWaiting = 1;	/* signal waiting for return */
				adbWaitingCmd = adbOutputBuffer[2];	/* save waiting command */
			} else {/* no talk, so done */
				/* set up stuff for adb_pass_up */
				memcpy(packet.data, adbInputBuffer,
				    adbInputBuffer[0] + 1);
				packet.saveBuf = adbBuffer;
				packet.compRout = adbCompRout;
				packet.compData = adbCompData;
				packet.cmd = adbWaitingCmd;
				packet.unsol = 0;
				packet.ack_only = 1;
				adb_pass_up(&packet);

				/* reset "waiting" vars, just in case */
				adbWaitingCmd = 0;
				adbBuffer = (long)0;
				adbCompRout = (long)0;
				adbCompData = (long)0;
			}

			adbWriteDelay = 0;	/* done writing */
			adbActionState = ADB_ACTION_IDLE;	/* signal bus is idle */
			ADB_SET_SR_INPUT();	/* make sure SR is set to IN */
			ADB_SET_STATE_INACTIVE();	/* end of frame */
		} else {
			ADB_SR() = adbOutputBuffer[adbSentChars + 1];	/* send next byte */
			ADB_SET_STATE_ACKON();	/* signal byte ready to shift */
		}
		break;

	case ADB_ACTION_NOTREADY:
#ifdef ADB_DEBUG
		if (adb_debug)
			printf_intr("adb: not yet initialized\n");
#endif
		break;

	default:
#ifdef ADB_DEBUG
		if (adb_debug)
			printf_intr("intr: unknown ADB state\n");
#endif
		break;
	}

	ADB_VIA_INTR_ENABLE();	/* enable ADB interrupt on IIs. */

	splx(s);		/* restore */

	return (1);
}				/* end adb_intr_IIsi */


/*****************************************************************************
 * if the device is currently busy, and there is no data waiting to go out, then
 * the data is "queued" in the outgoing buffer. If we are already waiting, then
 * we return.
 * in: if (in == 0) then the command string is built from command and buffer
 *     if (in != 0) then in is used as the command string
 * buffer: additional data to be sent (used only if in == 0)
 *         this is also where return data is stored
 * compRout: the completion routine that is called when then return value
 *	     is received (if a return value is expected)
 * data: a data pointer that can be used by the completion routine
 * command: an ADB command to be sent (used only if in == 0)
 *
 */
int
send_adb_IIsi(u_char *in, u_char *buffer, void *compRout, void *data, int
	command)
{
	int s, len;

	if (adbActionState == ADB_ACTION_NOTREADY)
		return 1;

	/* Don't interrupt while we are messing with the ADB */
	s = splhigh();

	if ((adbActionState == ADB_ACTION_IDLE) &&	/* ADB available? */
	    (ADB_INTR_IS_OFF)) {/* and no incoming interrupt? */

	} else
		if (adbWriteDelay == 0)	/* it's busy, but is anything waiting? */
			adbWriteDelay = 1;	/* if no, then we'll "queue"
						 * it up */
		else {
			splx(s);
			return 1;	/* really busy! */
		}

	if ((long)in == (long)0) {	/* need to convert? */
		/*
		 * Don't need to use adb_cmd_extra here because this section
		 * will be called ONLY when it is an ADB command (no RTC or
		 * PRAM)
		 */
		if ((command & 0x0c) == 0x08)	/* copy addl data ONLY if
						 * doing a listen! */
			len = buffer[0];	/* length of additional data */
		else
			len = 0;/* no additional data */

		adbOutputBuffer[0] = 2 + len;	/* dev. type + command + addl.
						 * data */
		adbOutputBuffer[1] = 0x00;	/* mark as an ADB command */
		adbOutputBuffer[2] = (u_char)command;	/* load command */

		/* copy additional output data, if any */
		memcpy(adbOutputBuffer + 3, buffer + 1, len);
	} else
		/* if data ready, just copy over */
		memcpy(adbOutputBuffer, in, in[0] + 2);

	adbSentChars = 0;	/* nothing sent yet */
	adbBuffer = buffer;	/* save buffer to know where to save result */
	adbCompRout = compRout;	/* save completion routine pointer */
	adbCompData = data;	/* save completion routine data pointer */
	adbWaitingCmd = adbOutputBuffer[2];	/* save wait command */

	if (adbWriteDelay != 1) {	/* start command now? */
		adbActionState = ADB_ACTION_OUT;	/* set next state */

		ADB_SET_STATE_ACTIVE();	/* tell ADB that we want to send */
		ADB_SET_STATE_ACKOFF();	/* make sure */

		ADB_SET_SR_OUTPUT();	/* set shift register for OUT */

		ADB_SR() = adbOutputBuffer[adbSentChars + 1];	/* load byte for output */

		ADB_SET_STATE_ACKON();	/* tell ADB byte ready to shift */
	}
	adbWriteDelay = 1;	/* something in the write "queue" */

	splx(s);

	if (0x0100 <= (s & 0x0700))	/* were VIA1 interrupts blocked? */
		/* poll until byte done */
		while ((adbActionState != ADB_ACTION_IDLE) || (ADB_INTR_IS_ON)
		    || (adbWaiting == 1))
			if (ADB_SR_INTR_IS_ON) { /* wait for "interrupt" */
				adb_intr_IIsi(NULL); /* go process it */
				if (adb_polling)
					adb_soft_intr();
			}

	 return 0;
}				/* send_adb_IIsi */

/* 
 * adb_pass_up is called by the interrupt-time routines.
 * It takes the raw packet data that was received from the
 * device and puts it into the queue that the upper half
 * processes. It then signals for a soft ADB interrupt which
 * will eventually call the upper half routine (adb_soft_intr).
 *
 * If in->unsol is 0, then this is either the notification
 * that the packet was sent (on a LISTEN, for example), or the 
 * response from the device (on a TALK). The completion routine
 * is called only if the user specified one.
 *
 * If in->unsol is 1, then this packet was unsolicited and
 * so we look up the device in the ADB device table to determine
 * what it's default service routine is.
 *
 * If in->ack_only is 1, then we really only need to call
 * the completion routine, so don't do any other stuff.
 *
 * Note that in->data contains the packet header AND data,
 * while adbInbound[]->data contains ONLY data.
 *
 * Note: Called only at interrupt time. Assumes this.
 */
void
adb_pass_up(struct adbCommand *in)
{
	int start = 0, len = 0, cmd = 0;
	ADBDataBlock block;

	/* temp for testing */
	/*u_char *buffer = 0;*/
	/*u_char *compdata = 0;*/
	/*u_char *comprout = 0;*/

	if (adbInCount >= ADB_QUEUE) {
#ifdef ADB_DEBUG
		if (adb_debug)
			printf_intr("adb: ring buffer overflow\n");
#endif
		return;
	}

	if (in->ack_only) {
		len = in->data[0];
		cmd = in->cmd;
		start = 0;
	} else {
		switch (adbHardware) {
		case ADB_HW_II:
			cmd = in->data[1];
			if (in->data[0] < 2)
				len = 0;
			else
				len = in->data[0]-1;
			start = 1;
			break;

		case ADB_HW_IISI:
		case ADB_HW_CUDA:
			/* If it's unsolicited, accept only ADB data for now */
			if (in->unsol)
				if (0 != in->data[2])
					return;
			cmd = in->data[4];
			if (in->data[0] < 5)
				len = 0;
			else
				len = in->data[0]-4;
			start = 4;
			break;

		case ADB_HW_PB:
			cmd = in->data[1];
			if (in->data[0] < 2)
				len = 0;
			else
				len = in->data[0]-1;
			start = 1;
			break;

		case ADB_HW_UNKNOWN:
			return;
		}

		/* Make sure there is a valid device entry for this device */
		if (in->unsol) {
			/* ignore unsolicited data during adbreinit */
			if (adbStarting)
				return;
			/* get device's comp. routine and data area */
			if (-1 == get_adb_info(&block, ADB_CMDADDR(cmd)))
				return;
		}
	}

	/*
 	 * If this is an unsolicited packet, we need to fill in
 	 * some info so adb_soft_intr can process this packet
 	 * properly. If it's not unsolicited, then use what
 	 * the caller sent us.
 	 */
	if (in->unsol) {
		adbInbound[adbInTail].compRout = (void *)block.dbServiceRtPtr;
		adbInbound[adbInTail].compData = (void *)block.dbDataAreaAddr;
		adbInbound[adbInTail].saveBuf = (void *)adbInbound[adbInTail].data;
	} else {
		adbInbound[adbInTail].compRout = (void *)in->compRout;
		adbInbound[adbInTail].compData = (void *)in->compData;
		adbInbound[adbInTail].saveBuf = (void *)in->saveBuf;
	}

#ifdef ADB_DEBUG
	if (adb_debug && in->data[1] == 2) 
		printf_intr("adb: caught error\n");
#endif

	/* copy the packet data over */
	/*
	 * TO DO: If the *_intr routines fed their incoming data
	 * directly into an adbCommand struct, which is passed to 
	 * this routine, then we could eliminate this copy.
	 */
	memcpy(adbInbound[adbInTail].data + 1, in->data + start + 1, len);
	adbInbound[adbInTail].data[0] = len;
	adbInbound[adbInTail].cmd = cmd;

	adbInCount++;
	if (++adbInTail >= ADB_QUEUE)
		adbInTail = 0;

	/*
	 * If the debugger is running, call upper half manually.
	 * Otherwise, trigger a soft interrupt to handle the rest later.
	 */
	if (adb_polling)
		adb_soft_intr();
	else
		setsoftadb();

	return;
}


/*
 * Called to process the packets after they have been
 * placed in the incoming queue.
 *
 */
void
adb_soft_intr(void)
{
	int s;
	int cmd = 0;
	u_char *buffer = 0;
	u_char *comprout = 0;
	u_char *compdata = 0;

/*delay(2*ADB_DELAY);*/

	while (adbInCount) {
#ifdef ADB_DEBUG
		if (adb_debug & 0x80)
			printf_intr("%x %x %x ",
			    adbInCount, adbInHead, adbInTail);
#endif
		/* get the data we need from the queue */
		buffer = adbInbound[adbInHead].saveBuf;
		comprout = adbInbound[adbInHead].compRout;
		compdata = adbInbound[adbInHead].compData;
		cmd = adbInbound[adbInHead].cmd;
	
		/* copy over data to data area if it's valid */
		/*
		 * Note that for unsol packets we don't want to copy the
	 	 * data anywhere, so buffer was already set to 0.
	 	 * For ack_only buffer was set to 0, so don't copy.
		 */
		if (buffer)
			memcpy(buffer, adbInbound[adbInHead].data,
			    adbInbound[adbInHead].data[0] + 1);

#ifdef ADB_DEBUG
			if (adb_debug & 0x80) {
				printf_intr("%p %p %p %x ",
				    buffer, comprout, compdata, (short)cmd);
				printf_intr("buf: ");
				print_single(adbInbound[adbInHead].data);
			}
#endif

		/* call default completion routine if it's valid */
		if (comprout) {
			(void)((int (*)(u_char *, u_char *, int))comprout)
			    (buffer, compdata, cmd);
		}

		s = splhigh();
		adbInCount--;
		if (++adbInHead >= ADB_QUEUE)
			adbInHead = 0;
		splx(s);

	}
	return;
}


/*
 * This is my version of the ADBOp routine. It mainly just calls the
 * hardware-specific routine.
 *
 *   data 	: pointer to data area to be used by compRout
 *   compRout	: completion routine
 *   buffer	: for LISTEN: points to data to send - MAX 8 data bytes,
 *		  byte 0 = # of bytes
 *		: for TALK: points to place to save return data
 *   command	: the adb command to send
 *   result	: 0 = success
 *		: -1 = could not complete
 */
int
adb_op(Ptr buffer, Ptr compRout, Ptr data, short command)
{
	int result;

	switch (adbHardware) {
	case ADB_HW_II:
		result = send_adb_II((u_char *)0, (u_char *)buffer,
		    (void *)compRout, (void *)data, (int)command);
		if (result == 0)
			return 0;
		else
			return -1;
		break;

	case ADB_HW_IISI:
		result = send_adb_IIsi((u_char *)0, (u_char *)buffer,
		    (void *)compRout, (void *)data, (int)command);
		/*
		 * I wish I knew why this delay is needed. It usually needs to
		 * be here when several commands are sent in close succession,
		 * especially early in device probes when doing collision
		 * detection. It must be some race condition. Sigh. - jpw
		 */
		delay(100);
		if (result == 0)
			return 0;
		else
			return -1;
		break;

	case ADB_HW_PB:
		result = pm_adb_op((u_char *)buffer, (void *)compRout,
		    (void *)data, (int)command);

		if (result == 0)
			return 0;
		else
			return -1;
		break;

	case ADB_HW_CUDA:
		result = send_adb_cuda((u_char *)0, (u_char *)buffer,
		    (void *)compRout, (void *)data, (int)command);
		if (result == 0)
			return 0;
		else
			return -1;
		break;

	case ADB_HW_UNKNOWN:
	default:
		return -1;
	}
}


/*
 * adb_hw_setup
 * This routine sets up the possible machine specific hardware
 * config (mainly VIA settings) for the various models.
 */
void
adb_hw_setup(void)
{
	volatile int i;
	u_char send_string[ADB_MAX_MSG_LENGTH];

	switch (adbHardware) {
	case ADB_HW_II:
		via1_register_irq(2, adb_intr_II, NULL, NULL);

		via_reg(VIA1, vDirB) |= 0x30;	/* register B bits 4 and 5:
						 * outputs */
		via_reg(VIA1, vDirB) &= 0xf7;	/* register B bit 3: input */
		via_reg(VIA1, vACR) &= ~vSR_OUT;	/* make sure SR is set
							 * to IN (II, IIsi) */
		adbActionState = ADB_ACTION_IDLE;	/* used by all types of
							 * hardware (II, IIsi) */
		adbBusState = ADB_BUS_IDLE;	/* this var. used in II-series
						 * code only */
		via_reg(VIA1, vIER) = 0x84;	/* make sure VIA interrupts
						 * are on (II, IIsi) */
		ADB_SET_STATE_IDLE_II();	/* set ADB bus state to idle */

		ADB_VIA_CLR_INTR();	/* clear interrupt */
		break;

	case ADB_HW_IISI:
		via1_register_irq(2, adb_intr_IIsi, NULL, NULL);
		via_reg(VIA1, vDirB) |= 0x30;	/* register B bits 4 and 5:
						 * outputs */
		via_reg(VIA1, vDirB) &= 0xf7;	/* register B bit 3: input */
		via_reg(VIA1, vACR) &= ~vSR_OUT;	/* make sure SR is set
							 * to IN (II, IIsi) */
		adbActionState = ADB_ACTION_IDLE;	/* used by all types of
							 * hardware (II, IIsi) */
		adbBusState = ADB_BUS_IDLE;	/* this var. used in II-series
						 * code only */
		via_reg(VIA1, vIER) = 0x84;	/* make sure VIA interrupts
						 * are on (II, IIsi) */
		ADB_SET_STATE_IDLE_IISI();	/* set ADB bus state to idle */

		/* get those pesky clock ticks we missed while booting */
		for (i = 0; i < 30; i++) {
			delay(ADB_DELAY);
			adb_hw_setup_IIsi(send_string);
#ifdef ADB_DEBUG
			if (adb_debug) {
				printf_intr("adb: cleanup: ");
				print_single(send_string);
			}
#endif
			delay(ADB_DELAY);
			if (ADB_INTR_IS_OFF)
				break;
		}
		break;

	case ADB_HW_PB:
		/*
		 * XXX - really PM_VIA_CLR_INTR - should we put it in
		 * pm_direct.h?
		 */
		pm_hw_setup();
		break;

	case ADB_HW_CUDA:
		via1_register_irq(2, adb_intr_cuda, NULL, NULL);
		via_reg(VIA1, vDirB) |= 0x30;	/* register B bits 4 and 5:
						 * outputs */
		via_reg(VIA1, vDirB) &= 0xf7;	/* register B bit 3: input */
		via_reg(VIA1, vACR) &= ~vSR_OUT;	/* make sure SR is set
							 * to IN */
		via_reg(VIA1, vACR) = (via_reg(VIA1, vACR) | 0x0c) & ~0x10;
		adbActionState = ADB_ACTION_IDLE;	/* used by all types of
							 * hardware */
		adbBusState = ADB_BUS_IDLE;	/* this var. used in II-series
						 * code only */
		via_reg(VIA1, vIER) = 0x84;	/* make sure VIA interrupts
						 * are on */
		ADB_SET_STATE_IDLE_CUDA();	/* set ADB bus state to idle */

		/* sort of a device reset */
		i = ADB_SR();	/* clear interrupt */
		ADB_VIA_INTR_DISABLE();	/* no interrupts while clearing */
		ADB_SET_STATE_IDLE_CUDA();	/* reset state to idle */
		delay(ADB_DELAY);
		ADB_SET_STATE_TIP();	/* signal start of frame */
		delay(ADB_DELAY);
		ADB_TOGGLE_STATE_ACK_CUDA();
		delay(ADB_DELAY);
		ADB_CLR_STATE_TIP();
		delay(ADB_DELAY);
		ADB_SET_STATE_IDLE_CUDA();	/* back to idle state */
		i = ADB_SR();	/* clear interrupt */
		ADB_VIA_INTR_ENABLE();	/* ints ok now */
		break;

	case ADB_HW_UNKNOWN:
	default:
		via_reg(VIA1, vIER) = 0x04;	/* turn interrupts off - TO
						 * DO: turn PB ints off? */
		return;
		break;
	}
}


/*
 * adb_hw_setup_IIsi
 * This is sort of a "read" routine that forces the adb hardware through a read cycle
 * if there is something waiting. This helps "clean up" any commands that may have gotten
 * stuck or stopped during the boot process.
 *
 */
void
adb_hw_setup_IIsi(u_char *buffer)
{
	int i;
	int dummy;
	int s;
	long my_time;
	int endofframe;

	delay(ADB_DELAY);

	i = 1;			/* skip over [0] */
	s = splhigh();		/* block ALL interrupts while we are working */
	ADB_SET_SR_INPUT();	/* make sure SR is set to IN */
	ADB_VIA_INTR_DISABLE();	/* disable ADB interrupt on IIs. */
	/* this is required, especially on faster machines */
	delay(ADB_DELAY);

	if (ADB_INTR_IS_ON) {
		ADB_SET_STATE_ACTIVE();	/* signal start of data frame */

		endofframe = 0;
		while (0 == endofframe) {
			/*
			 * Poll for ADB interrupt and watch for timeout.
			 * If time out, keep going in hopes of not hanging
			 * the ADB chip - I think
			 */
			my_time = ADB_DELAY * 5;
			while ((ADB_SR_INTR_IS_OFF) && (my_time-- > 0))
				dummy = via_reg(VIA1, vBufB);

			buffer[i++] = ADB_SR();	/* reset interrupt flag by
						 * reading vSR */
			/*
			 * Perhaps put in a check here that ignores all data
			 * after the first ADB_MAX_MSG_LENGTH bytes ???
			 */
			if (ADB_INTR_IS_OFF)	/* check for end of frame */
				endofframe = 1;

			ADB_SET_STATE_ACKON();	/* send ACK to ADB chip */
			delay(ADB_DELAY);	/* delay */
			ADB_SET_STATE_ACKOFF();	/* send ACK to ADB chip */
		}
		ADB_SET_STATE_INACTIVE();	/* signal end of frame and
						 * delay */

		/* probably don't need to delay this long */
		delay(ADB_DELAY);
	}
	buffer[0] = --i;	/* [0] is length of message */
	ADB_VIA_INTR_ENABLE();	/* enable ADB interrupt on IIs. */
	splx(s);		/* restore interrupts */

	return;
}				/* adb_hw_setup_IIsi */



/*
 * adb_reinit sets up the adb stuff
 *
 */
void
adb_reinit(void)
{
	u_char send_string[ADB_MAX_MSG_LENGTH];
	ADBDataBlock data;	/* temp. holder for getting device info */
	volatile int i, x;
	int s;
	int command;
	int result;
	int saveptr;		/* point to next free relocation address */
	int device;
	int nonewtimes;		/* times thru loop w/o any new devices */

	via1_register_irq(2, adb_intr, NULL, "adb");
	adb_setup_hw_type();	/* setup hardware type */

	/* Make sure we are not interrupted while building the table. */
	/* ints must be on for PB & IOP (at least, for now) */
	if (adbHardware != ADB_HW_PB)
		s = splhigh();
	else
		s = 0;		/* XXX shut the compiler up*/

	ADBNumDevices = 0;	/* no devices yet */

	/* Let intr routines know we are running reinit */
	adbStarting = 1;

	/*
	 * Initialize the ADB table.  For now, we'll always use the same table
	 * that is defined at the beginning of this file - no mallocs.
	 */
	for (i = 0; i < 16; i++) {
		ADBDevTable[i].devType = 0;
		ADBDevTable[i].origAddr = ADBDevTable[i].currentAddr = 0;
	}

	adb_hw_setup();		/* init the VIA bits and hard reset ADB */

	delay(1000);

	/* send an ADB reset first */
	(void)adb_op_sync((Ptr)0, (Ptr)0, (Ptr)0, (short)0x00);
	delay(3000);

	/*
	 * Probe for ADB devices. Probe devices 1-15 quickly to determine
	 * which device addresses are in use and which are free. For each
	 * address that is in use, move the device at that address to a higher
	 * free address. Continue doing this at that address until no device
	 * responds at that address. Then move the last device that was moved
	 * back to the original address. Do this for the remaining addresses
	 * that we determined were in use.
	 * 
	 * When finished, do this entire process over again with the updated
	 * list of in use addresses. Do this until no new devices have been
	 * found in 20 passes though the in use address list. (This probably
	 * seems long and complicated, but it's the best way to detect multiple
	 * devices at the same address - sometimes it takes a couple of tries
	 * before the collision is detected.)
	 */

	/* initial scan through the devices */
	for (i = 1; i < 16; i++) {
		command = ADBTALK(i, 3);
		result = adb_op_sync((Ptr)send_string, (Ptr)0,
		    (Ptr)0, (short)command);

		if (result == 0 && send_string[0] != 0) {
			/* found a device */
			++ADBNumDevices;
			KASSERT(ADBNumDevices < 16);
			ADBDevTable[ADBNumDevices].devType =
				(int)(send_string[2]);
			ADBDevTable[ADBNumDevices].origAddr = i;
			ADBDevTable[ADBNumDevices].currentAddr = i;
			ADBDevTable[ADBNumDevices].DataAreaAddr =
			    (long)0;
			ADBDevTable[ADBNumDevices].ServiceRtPtr = (void *)0;
			pm_check_adb_devices(i);	/* tell pm driver device
							 * is here */
		}
	}

	/* find highest unused address */
	for (saveptr = 15; saveptr > 0; saveptr--)
		if (-1 == get_adb_info(&data, saveptr))
			break;

#ifdef ADB_DEBUG
	if (adb_debug & 0x80) {
		printf_intr("first free is: 0x%02x\n", saveptr);
		printf_intr("devices: %i\n", ADBNumDevices);
	}
#endif

	nonewtimes = 0;		/* no loops w/o new devices */
	while (saveptr > 0 && nonewtimes++ < 11) {
		for (i = 1;saveptr > 0 && i <= ADBNumDevices; i++) {
			device = ADBDevTable[i].currentAddr;
#ifdef ADB_DEBUG
			if (adb_debug & 0x80)
				printf_intr("moving device 0x%02x to 0x%02x "
				    "(index 0x%02x)  ", device, saveptr, i);
#endif

			/* send TALK R3 to address */
			command = ADBTALK(device, 3);
			(void)adb_op_sync((Ptr)send_string, (Ptr)0,
			    (Ptr)0, (short)command);

			/* move device to higher address */
			command = ADBLISTEN(device, 3);
			send_string[0] = 2;
			send_string[1] = (u_char)(saveptr | 0x60);
			send_string[2] = 0xfe;
			(void)adb_op_sync((Ptr)send_string, (Ptr)0,
			    (Ptr)0, (short)command);
			delay(1000);

			/* send TALK R3 - anthing at new address? */
			command = ADBTALK(saveptr, 3);
			send_string[0] = 0;
			result = adb_op_sync((Ptr)send_string, (Ptr)0,
			    (Ptr)0, (short)command);
			delay(1000);

			if (result != 0 || send_string[0] == 0) {
				/*
				 * maybe there's a communication breakdown;
				 * just in case, move it back from whence it
				 * came, and we'll try again later
				 */
				command = ADBLISTEN(saveptr, 3);
				send_string[0] = 2;
				send_string[1] = (u_char)(device | 0x60);
				send_string[2] = 0x00;
				(void)adb_op_sync((Ptr)send_string, (Ptr)0,
				    (Ptr)0, (short)command);
#ifdef ADB_DEBUG
				if (adb_debug & 0x80)
					printf_intr("failed, continuing\n");
#endif
				delay(1000);
				continue;
			}

			/* send TALK R3 - anything at old address? */
			command = ADBTALK(device, 3);
			send_string[0] = 0;
			result = adb_op_sync((Ptr)send_string, (Ptr)0,
			    (Ptr)0, (short)command);
			if (result == 0 && send_string[0] != 0) {
				/* new device found */
				/* update data for previously moved device */
				ADBDevTable[i].currentAddr = saveptr;
#ifdef ADB_DEBUG
				if (adb_debug & 0x80)
					printf_intr("old device at index %i\n",i);
#endif
				/* add new device in table */
#ifdef ADB_DEBUG
				if (adb_debug & 0x80)
					printf_intr("new device found\n");
#endif
				if (saveptr > ADBNumDevices) {
					++ADBNumDevices;
					KASSERT(ADBNumDevices < 16);
				}
				ADBDevTable[ADBNumDevices].devType =
					(int)(send_string[2]);
				ADBDevTable[ADBNumDevices].origAddr = device;
				ADBDevTable[ADBNumDevices].currentAddr = device;
				/* These will be set correctly in adbsys.c */
				/* Until then, unsol. data will be ignored. */
				ADBDevTable[ADBNumDevices].DataAreaAddr =
				    (long)0;
				ADBDevTable[ADBNumDevices].ServiceRtPtr =
				    (void *)0;
				/* find next unused address */
				for (x = saveptr; x > 0; x--) {
					if (-1 == get_adb_info(&data, x)) {
						saveptr = x;
						break;
					}
				}
				if (x == 0)
					saveptr = 0;
#ifdef ADB_DEBUG
				if (adb_debug & 0x80)
					printf_intr("new free is 0x%02x\n",
					    saveptr);
#endif
				nonewtimes = 0;
				/* tell pm driver device is here */
				pm_check_adb_devices(device);
			} else {
#ifdef ADB_DEBUG
				if (adb_debug & 0x80)
					printf_intr("moving back...\n");
#endif
				/* move old device back */
				command = ADBLISTEN(saveptr, 3);
				send_string[0] = 2;
				send_string[1] = (u_char)(device | 0x60);
				send_string[2] = 0xfe;
				(void)adb_op_sync((Ptr)send_string, (Ptr)0,
				    (Ptr)0, (short)command);
				delay(1000);
			}
		}
	}

#ifdef ADB_DEBUG
	if (adb_debug) {
		for (i = 1; i <= ADBNumDevices; i++) {
			x = get_ind_adb_info(&data, i);
			if (x != -1)
				printf_intr("index 0x%x, addr 0x%x, type 0x%hx\n",
				    i, x, data.devType);
		}
	}
#endif

	/* enable the programmer's switch, if we have one */
	adb_prog_switch_enable();

#ifdef ADB_DEBUG
	if (adb_debug) {
		if (0 == ADBNumDevices)	/* tell user if no devices found */
			printf_intr("adb: no devices found\n");
	}
#endif

	adbStarting = 0;	/* not starting anymore */
#ifdef ADB_DEBUG
	if (adb_debug)
		printf_intr("adb: adb_reinit complete\n");
#endif

	if (adbHardware == ADB_HW_CUDA) {
		timeout_set(&adb_cuda_timeout, (void *)adb_cuda_tickle, NULL);
		timeout_add(&adb_cuda_timeout, ADB_TICKLE_TICKS);
	}

	/* ints must be on for PB & IOP (at least, for now) */
	if (adbHardware != ADB_HW_PB)
		splx(s);

	return;
}


/*
 * adb_cmd_result
 *
 * This routine lets the caller know whether the specified adb command string
 * should expect a returned result, such as a TALK command.
 *
 * returns: 0 if a result should be expected
 *          1 if a result should NOT be expected
 */
int
adb_cmd_result(u_char *in)
{
	switch (adbHardware) {
	case ADB_HW_II:
		/* was it an ADB talk command? */
		if ((in[1] & 0x0c) == 0x0c)
			return 0;
		return 1;

	case ADB_HW_IISI:
	case ADB_HW_CUDA:
		/* was it an ADB talk command? */
		if ((in[1] == 0x00) && ((in[2] & 0x0c) == 0x0c))
			return 0;
		/* was it an RTC/PRAM read date/time? */
		if ((in[1] == 0x01) && (in[2] == 0x03))
			return 0;
		return 1;

	case ADB_HW_PB:
		return 1;

	case ADB_HW_UNKNOWN:
	default:
		return 1;
	}
}

void 
adb_setup_hw_type(void)
{
	long response;

	response = mac68k_machine.machineid;

	/*
	 * Determine what type of ADB hardware we are running on.
	 */
	switch (response) {
	case MACH_MACC610:		/* Centris 610 */
	case MACH_MACC650:		/* Centris 650 */
	case MACH_MACII:		/* II */
	case MACH_MACIICI:		/* IIci */
	case MACH_MACIICX:		/* IIcx */
	case MACH_MACIIX:		/* IIx */
	case MACH_MACQ610:		/* Quadra 610 */
	case MACH_MACQ650:		/* Quadra 650 */
	case MACH_MACQ700:		/* Quadra 700 */
	case MACH_MACQ800:		/* Quadra 800 */
	case MACH_MACSE30:		/* SE/30 */
		adbHardware = ADB_HW_II;
#ifdef ADB_DEBUG
		if (adb_debug)
			printf_intr("adb: using II series hardware support\n");
#endif
		break;

	case MACH_MACCLASSICII:		/* Classic II */
	case MACH_MACLCII:		/* LC II, Performa 400/405/430 */
	case MACH_MACLCIII:		/* LC III, Performa 450 */
	case MACH_MACIISI:		/* IIsi */
	case MACH_MACIIVI:		/* IIvi */
	case MACH_MACIIVX:		/* IIvx */
	case MACH_MACP460:		/* Performa 460/465/467 */
	case MACH_MACP600:		/* Performa 600 */
		adbHardware = ADB_HW_IISI;
#ifdef ADB_DEBUG
		if (adb_debug)
			printf_intr("adb: using IIsi series hardware support\n");
#endif
		break;

	case MACH_MACPB140:		/* PowerBook 140 */
	case MACH_MACPB145:		/* PowerBook 145 */
	case MACH_MACPB160:		/* PowerBook 160 */
	case MACH_MACPB165:		/* PowerBook 165 */
	case MACH_MACPB165C:		/* PowerBook 165c */
	case MACH_MACPB170:		/* PowerBook 170 */
	case MACH_MACPB180:		/* PowerBook 180 */
	case MACH_MACPB180C:		/* PowerBook 180c */
		adbHardware = ADB_HW_PB;
		pm_setup_adb();
#ifdef ADB_DEBUG
		if (adb_debug)
			printf_intr("adb: using PowerBook 100-series hardware support\n");
#endif
		break;

	case MACH_MACPB150:		/* PowerBook 150 */
	case MACH_MACPB210:		/* PowerBook Duo 210 */
	case MACH_MACPB230:		/* PowerBook Duo 230 */
	case MACH_MACPB250:		/* PowerBook Duo 250 */
	case MACH_MACPB270:		/* PowerBook Duo 270 */
	case MACH_MACPB280:		/* PowerBook Duo 280 */
	case MACH_MACPB280C:		/* PowerBook Duo 280c */
	case MACH_MACPB500:		/* PowerBook 500 series */
	case MACH_MACPB190:		/* PowerBook 190 */
	case MACH_MACPB190CS:		/* PowerBook 190cs */
		adbHardware = ADB_HW_PB;
		pm_setup_adb();
#ifdef ADB_DEBUG
		if (adb_debug)
			printf_intr("adb: using PowerBook Duo-series and PowerBook 500-series hardware support\n");
#endif
		break;

	case MACH_MACC660AV:		/* Centris 660AV */
	case MACH_MACCCLASSIC:		/* Color Classic */
	case MACH_MACCCLASSICII:	/* Color Classic II */
	case MACH_MACLC475:		/* LC 475, Performa 475/476 */
	case MACH_MACLC475_33:		/* Clock-chipped 47x */
	case MACH_MACLC520:		/* LC 520 */
	case MACH_MACLC575:		/* LC 575, Performa 575/577/578 */
	case MACH_MACP550:		/* LC 550, Performa 550 */
	case MACH_MACTV:		/* Macintosh TV */
	case MACH_MACP580:		/* Performa 580/588 */
	case MACH_MACQ605:		/* Quadra 605 */
	case MACH_MACQ605_33:		/* Clock-chipped Quadra 605 */
	case MACH_MACQ630:		/* LC 630, Performa 630, Quadra 630 */
	case MACH_MACQ840AV:		/* Quadra 840AV */
		adbHardware = ADB_HW_CUDA;
#ifdef ADB_DEBUG
		if (adb_debug)
			printf_intr("adb: using Cuda series hardware support\n");
#endif
		break;

	default:
		adbHardware = ADB_HW_UNKNOWN;
#ifdef ADB_DEBUG
		if (adb_debug) {
			printf_intr("adb: hardware type unknown for this machine\n");
			printf_intr("adb: ADB support is disabled\n");
		}
#endif
		break;
	}

	/*
	 * Determine whether this machine has ADB based soft power.
	 */
	switch (response) {
	case MACH_MACCCLASSIC:		/* Color Classic */
	case MACH_MACCCLASSICII:	/* Color Classic II */
	case MACH_MACIISI:		/* IIsi */
	case MACH_MACIIVI:		/* IIvi */
	case MACH_MACIIVX:		/* IIvx */
	case MACH_MACLC520:		/* LC 520 */
	case MACH_MACLC575:		/* LC 575, Performa 575/577/578 */
	case MACH_MACP550:		/* LC 550, Performa 550 */
	case MACH_MACTV:		/* Macintosh TV */
	case MACH_MACP580:		/* Performa 580/588 */
	case MACH_MACP600:		/* Performa 600 */
	case MACH_MACQ630:		/* LC 630, Performa 630, Quadra 630 */
	case MACH_MACQ840AV:		/* Quadra 840AV */
		adbSoftPower = 1;
		break;
	}
}

/*
 * adb_op_sync
 *
 * This routine does exactly what the adb_op routine does, except that after
 * the adb_op is called, it waits until the return value is present before
 * returning.
 *
 * NOTE: The user specified compRout is ignored, since this routine specifies
 * it's own to adb_op, which is why you really called this in the first place
 * anyway.
 */
int
adb_op_sync(Ptr buffer, Ptr compRout, Ptr data, short command)
{
	int tmout;
	int result;
	volatile int flag = 0;

	result = adb_op(buffer, (void *)adb_op_comprout, (Ptr)&flag, 
	    command);	/* send command */
	if (result == 0) {		/* send ok? */
		/*
		 * Total time to wait is calculated as follows:
		 *  - Tlt (stop to start time): 260 usec
		 *  - start bit: 100 usec
		 *  - up to 8 data bytes: 64 * 100 usec = 6400 usec
		 *  - stop bit (with SRQ): 140 usec
		 * Total: 6900 usec
		 *
		 * This is the total time allowed by the specification.  Any
		 * device that doesn't conform to this will fail to operate
		 * properly on some Apple systems.  In spite of this we
		 * double the time to wait; some Cuda-based apparently
		 * queues some commands and allows the main CPU to continue
		 * processing (radical concept, eh?).  To be safe, allow
		 * time for two complete ADB transactions to occur.
		 */
		for (tmout = 13800; !flag && tmout >= 10; tmout -= 10)
			delay(10);
		if (!flag && tmout > 0)
			delay(tmout);

		if (!flag)
			result = -2;
	}

	return result;
}

/*
 * adb_op_comprout
 *
 * This function is used by the adb_op_sync routine so it knows when the
 * function is done.
 */
void 
adb_op_comprout(caddr_t buffer, caddr_t data_area, int adb_command)
{
	*(u_short *)data_area = 0x01;		/* update flag value */
}
	
int 
count_adbs(void)
{
	int i;
	int found;

	found = 0;

	for (i = 1; i < 16; i++)
		if (0 != ADBDevTable[i].currentAddr)
			found++;

	return found;
}

int 
get_ind_adb_info(ADBDataBlock *info, int index)
{
	if ((index < 1) || (index > 15))	/* check range 1-15 */
		return (-1);

#ifdef ADB_DEBUG
	if (adb_debug & 0x80)
		printf_intr("index 0x%x devType is: 0x%x\n", index,
		    ADBDevTable[index].devType);
#endif
	if (0 == ADBDevTable[index].devType)	/* make sure it's a valid entry */
		return (-1);

	info->devType = (unsigned char)(ADBDevTable[index].devType);
	info->origADBAddr = (unsigned char)(ADBDevTable[index].origAddr);
	info->dbServiceRtPtr = (Ptr)ADBDevTable[index].ServiceRtPtr;
	info->dbDataAreaAddr = (Ptr)ADBDevTable[index].DataAreaAddr;

	return (ADBDevTable[index].currentAddr);
}

int 
get_adb_info(ADBDataBlock *info, int adbAddr)
{
	int i;

	if ((adbAddr < 1) || (adbAddr > 15))	/* check range 1-15 */
		return (-1);

	for (i = 1; i < 15; i++)
		if (ADBDevTable[i].currentAddr == adbAddr) {
			info->devType = (unsigned char)(ADBDevTable[i].devType);
			info->origADBAddr = (unsigned char)(ADBDevTable[i].origAddr);
			info->dbServiceRtPtr = (Ptr)ADBDevTable[i].ServiceRtPtr;
			info->dbDataAreaAddr = ADBDevTable[i].DataAreaAddr;
			return 0;	/* found */
		}

	return (-1);		/* not found */
}

int 
set_adb_info(ADBSetInfoBlock *info, int adbAddr)
{
	int i;

	if ((adbAddr < 1) || (adbAddr > 15))	/* check range 1-15 */
		return (-1);

	for (i = 1; i < 15; i++)
		if (ADBDevTable[i].currentAddr == adbAddr) {
			ADBDevTable[i].ServiceRtPtr =
			    (void *)(info->siServiceRtPtr);
			ADBDevTable[i].DataAreaAddr = info->siDataAreaAddr;
			return 0;	/* found */
		}

	return (-1);		/* not found */

}

/* caller should really use machine-independant version: getPramTime */
/* this version does pseudo-adb access only */
int 
adb_read_date_time(unsigned long *time)
{
	u_char output[ADB_MAX_MSG_LENGTH];
	int result;
	volatile int flag = 0;

	switch (adbHardware) {
	case ADB_HW_II:
		return -1;

	case ADB_HW_IISI:
		output[0] = 0x02;	/* 2 byte message */
		output[1] = 0x01;	/* to pram/rtc device */
		output[2] = 0x03;	/* read date/time */
		result = send_adb_IIsi((u_char *)output, (u_char *)output,
		    (void *)adb_op_comprout, (void *)&flag, (int)0);
		if (result != 0)	/* exit if not sent */
			return -1;

		while (0 == flag)	/* wait for result */
			;

		*time = (long)(*(long *)(output + 1));
		return 0;

	case ADB_HW_PB:
		return -1;

	case ADB_HW_CUDA:
		output[0] = 0x02;	/* 2 byte message */
		output[1] = 0x01;	/* to pram/rtc device */
		output[2] = 0x03;	/* read date/time */
		result = send_adb_cuda((u_char *)output, (u_char *)output,
		    (void *)adb_op_comprout, (void *)&flag, (int)0);
		if (result != 0)	/* exit if not sent */
			return -1;

		while (0 == flag)	/* wait for result */
			;

		*time = (long)(*(long *)(output + 1));
		return 0;

	case ADB_HW_UNKNOWN:
	default:
		return -1;
	}
}

/* caller should really use machine-independant version: setPramTime */
/* this version does pseudo-adb access only */
int 
adb_set_date_time(unsigned long time)
{
	u_char output[ADB_MAX_MSG_LENGTH];
	int result;
	volatile int flag = 0;

	switch (adbHardware) {
	case ADB_HW_II:
		return -1;

	case ADB_HW_IISI:
		output[0] = 0x06;	/* 6 byte message */
		output[1] = 0x01;	/* to pram/rtc device */
		output[2] = 0x09;	/* set date/time */
		output[3] = (u_char)(time >> 24);
		output[4] = (u_char)(time >> 16);
		output[5] = (u_char)(time >> 8);
		output[6] = (u_char)(time);
		result = send_adb_IIsi((u_char *)output, (u_char *)0,
		    (void *)adb_op_comprout, (void *)&flag, (int)0);
		if (result != 0)	/* exit if not sent */
			return -1;

		while (0 == flag)	/* wait for send to finish */
			;

		return 0;

	case ADB_HW_PB:
		return -1;

	case ADB_HW_CUDA:
		output[0] = 0x06;	/* 6 byte message */
		output[1] = 0x01;	/* to pram/rtc device */
		output[2] = 0x09;	/* set date/time */
		output[3] = (u_char)(time >> 24);
		output[4] = (u_char)(time >> 16);
		output[5] = (u_char)(time >> 8);
		output[6] = (u_char)(time);
		result = send_adb_cuda((u_char *)output, (u_char *)0,
		    (void *)adb_op_comprout, (void *)&flag, (int)0);
		if (result != 0)	/* exit if not sent */
			return -1;

		while (0 == flag)	/* wait for send to finish */
			;

		return 0;

	case ADB_HW_UNKNOWN:
	default:
		return -1;
	}
}


int 
adb_poweroff(void)
{
	u_char output[ADB_MAX_MSG_LENGTH];
	int result;

	if (!adbSoftPower)
		return -1;

	adb_polling = 1;

	switch (adbHardware) {
	case ADB_HW_IISI:
		output[0] = 0x02;	/* 2 byte message */
		output[1] = 0x01;	/* to pram/rtc/soft-power device */
		output[2] = 0x0a;	/* set date/time */
		result = send_adb_IIsi((u_char *)output, (u_char *)0,
		    (void *)0, (void *)0, (int)0);
		if (result != 0)	/* exit if not sent */
			return -1;

		for (;;);		/* wait for power off */

		return 0;

	case ADB_HW_PB:
		return -1;

	case ADB_HW_CUDA:
		output[0] = 0x02;	/* 2 byte message */
		output[1] = 0x01;	/* to pram/rtc/soft-power device */
		output[2] = 0x0a;	/* set date/time */
		result = send_adb_cuda((u_char *)output, (u_char *)0,
		    (void *)0, (void *)0, (int)0);
		if (result != 0)	/* exit if not sent */
			return -1;

		for (;;);		/* wait for power off */

		return 0;

	case ADB_HW_II:			/* II models don't do ADB soft power */
	case ADB_HW_UNKNOWN:
	default:
		return -1;
	}
}

int 
adb_prog_switch_enable(void)
{
	u_char output[ADB_MAX_MSG_LENGTH];
	int result;
	volatile int flag = 0;

	switch (adbHardware) {
	case ADB_HW_IISI:
		output[0] = 0x03;	/* 3 byte message */
		output[1] = 0x01;	/* to pram/rtc/soft-power device */
		output[2] = 0x1c;	/* prog. switch control */
		output[3] = 0x01;	/* enable */
		result = send_adb_IIsi((u_char *)output, (u_char *)0,
		    (void *)adb_op_comprout, (void *)&flag, (int)0);
		if (result != 0)	/* exit if not sent */
			return -1;

		while (0 == flag)	/* wait for send to finish */
			;

		return 0;

	case ADB_HW_PB:
		return -1;

	case ADB_HW_II:		/* II models don't do prog. switch */
	case ADB_HW_CUDA:	/* cuda doesn't do prog. switch TO DO: verify this */
	case ADB_HW_UNKNOWN:
	default:
		return -1;
	}
}

int 
adb_prog_switch_disable(void)
{
	u_char output[ADB_MAX_MSG_LENGTH];
	int result;
	volatile int flag = 0;

	switch (adbHardware) {
	case ADB_HW_IISI:
		output[0] = 0x03;	/* 3 byte message */
		output[1] = 0x01;	/* to pram/rtc/soft-power device */
		output[2] = 0x1c;	/* prog. switch control */
		output[3] = 0x01;	/* disable */
		result = send_adb_IIsi((u_char *)output, (u_char *)0,
			(void *)adb_op_comprout, (void *)&flag, (int)0);
		if (result != 0)	/* exit if not sent */
			return -1;

		while (0 == flag)	/* wait for send to finish */
			;

		return 0;

	case ADB_HW_PB:
		return -1;

	case ADB_HW_II:		/* II models don't do prog. switch */
	case ADB_HW_CUDA:	/* cuda doesn't do prog. switch */
	case ADB_HW_UNKNOWN:
	default:
		return -1;
	}
}

/*
 * Function declarations.
 */
int	adbmatch(struct device *, void *, void *);
void	adbattach(struct device *, struct device *, void *);
void	adb_attach_deferred(void *);

/*
 * Driver definition.
 */
struct cfattach adb_ca = {
	sizeof(struct device), adbmatch, adbattach
};

int
adbmatch(struct device *parent, void *vcf, void *aux)
{
	static int adb_matched = 0;

	/* Allow only one instance. */
	if (adb_matched)
		return (0);

	adb_matched = 1;
	return (1);
}

void
adbattach(struct device *parent, struct device *self, void *aux)
{
	printf("\n");
	startuphook_establish(adb_attach_deferred, self);
}

void
adb_attach_deferred(void *v)
{
	struct device *self = v;
	ADBDataBlock adbdata;
	struct adb_attach_args aa_args;
	int totaladbs;
	int adbindex, adbaddr;

	printf("%s", self->dv_xname);

	adb_polling = 1;
	adb_reinit();

	printf(": %s", adbHardwareDescr[adbHardware]);

#ifdef ADB_DEBUG
	if (adb_debug)
		printf("adb: done with adb_reinit\n");
#endif

	totaladbs = count_adbs();

	printf(", %d target%s\n", totaladbs, (totaladbs == 1) ? "" : "s");

	/* for each ADB device */
	for (adbindex = 1; adbindex <= totaladbs; adbindex++) {
		/* Get the ADB information */
		adbaddr = get_ind_adb_info(&adbdata, adbindex);

		aa_args.origaddr = (int)(adbdata.origADBAddr);
		aa_args.adbaddr = adbaddr;
		aa_args.handler_id = (int)(adbdata.devType);

		(void)config_found(self, &aa_args, adbprint);
	}
	adb_polling = 0;
}
