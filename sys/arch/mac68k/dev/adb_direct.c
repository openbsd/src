/*	$OpenBSD: adb_direct.c,v 1.15 2004/11/26 21:21:23 miod Exp $	*/
/*	$NetBSD: adb_direct.c,v 1.5 1997/04/21 18:04:28 scottr Exp $	*/

/*  From: adb_direct.c 2.02 4/18/97 jpw */

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

/* This code is rather messy, but I don't have time right now
 * to clean it up as much as I would like.
 * But it works, so I'm happy. :-) jpw */

#if defined(__NetBSD__) || defined(__OpenBSD__)

#include <sys/param.h>
#include <sys/cdefs.h>
#include <sys/systm.h>

#include <machine/viareg.h>
#include <machine/param.h>
#include <machine/cpu.h>
#include <machine/adbsys.h>			/* required for adbvar.h */

#include <arch/mac68k/mac68k/macrom.h>
#include "adbvar.h"
#define printf_intr printf
#else
#include "via.h"				/* for macos based testing */
typedef unsigned char	u_char;
#endif


#ifdef MRG_ADB
int     adb_poweroff(void);
int     adb_read_date_time(unsigned long *t);
int     adb_set_date_time(unsigned long t);
#endif

/* more verbose for testing */
/*#define DEBUG*/

/* some misc. leftovers */
#define vPB		0x0000
#define vPB3		0x08
#define vPB4		0x10
#define vPB5		0x20
#define vSR_INT		0x04
#define vSR_OUT		0x10

/* types of adb hardware that we (will eventually) support */
#define ADB_HW_UNKNOWN		0x01	/* don't know */
#define ADB_HW_II		0x02	/* Mac II series */
#define ADB_HW_IISI		0x03	/* Mac IIsi series */
#define ADB_HW_PB		0x04	/* PowerBook series */
#define ADB_HW_CUDA		0x05	/* Machines with a Cuda chip */

/* the type of ADB action that we are currently preforming */
#define ADB_ACTION_NOTREADY	0x01	/* has not been initialized yet */
#define ADB_ACTION_IDLE		0x02	/* the bus is currently idle */
#define ADB_ACTION_OUT		0x03	/* sending out a command */
#define ADB_ACTION_IN		0x04	/* receiving data */

/*
 * These describe the state of the ADB bus itself, although they
 * don't necessarily correspond directly to ADB states.
 * Note: these are not really used in the IIsi code.
 */
#define ADB_BUS_UNKNOWN		0x01	/* we don't know yet - all models */
#define ADB_BUS_IDLE		0x02	/* bus is idle - all models */
#define ADB_BUS_CMD		0x03	/* starting a command - II models */
#define ADB_BUS_ODD		0x04	/* the "odd" state - II models */
#define ADB_BUS_EVEN		0x05	/* the "even" state - II models */
#define ADB_BUS_ACTIVE		0x06	/* active state - IIsi models */
#define ADB_BUS_ACK		0x07	/* currently ACKing - IIsi models */

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
						vBufB) | vPB5) & ~vPB4 )
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
#define MAX_ADB_MSG_LENGTH	20

/*
 * A structure for storing information about each ADB device.
 */
struct ADBDevEntry {
	void	(*ServiceRtPtr)(void);
	void	*DataAreaAddr;
	char	devType;
	char	origAddr;
	char	currentAddr;
};

/*
 * Used to hold ADB commands that are waiting to be sent out.
 */
struct adbCmdHoldEntry {
	u_char	outBuf[MAX_ADB_MSG_LENGTH];	/* our message */
	u_char	*saveBuf;	/* buffer to know where to save result */
	u_char	*compRout;	/* completion routine pointer */
	u_char	*data;		/* completion routine data pointer */
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
int	adbNextEnd = 0;		/* the next incoming bute is the last (II) */

int	adbWaitingCmd = 0;	/* ADB command we are waiting for */
u_char	*adbBuffer = (long) 0;	/* pointer to user data area */
void	*adbCompRout = (long) 0;	/* pointer to the completion routine */
void	*adbCompData = (long) 0;	/* pointer to the completion routine data */
long	adbFakeInts = 0;	/* keeps track of fake ADB interrupts for
				 * timeouts (II) */
int	adbStarting = 1;	/* doing ADBReInit so do polling differently */
int	adbSendTalk = 0;	/* the intr routine is sending the talk, not
				 * the user (II) */
int	adbPolling = 0;		/* we are polling for service request */
int	adbPollCmd = 0;		/* the last poll command we sent */

u_char	adbInputBuffer[MAX_ADB_MSG_LENGTH];	/* data input buffer */
u_char	adbOutputBuffer[MAX_ADB_MSG_LENGTH];	/* data output buffer */
struct	adbCmdHoldEntry adbOutQueue;		/* our 1 entry output queue */

int	adbSentChars = 0;	/* how many characters we have sent */
int	adbLastDevice = 0;	/* last ADB dev we heard from (II ONLY) */
int	adbLastDevIndex = 0;	/* last ADB dev loc in dev table (II ONLY) */
int	adbLastCommand = 0;	/* the last ADB command we sent (II) */

struct ADBDevEntry ADBDevTable[16];	/* our ADB device table */
int	ADBNumDevices;		/* num. of ADB devices found with ADBReInit */

extern struct mac68k_machine_S mac68k_machine;

void	pm_setup_adb(void);
void	pm_check_adb_devices(int);
void	pm_intr(void);
int	pm_adb_op(u_char *, void *, void *, int);
void	pm_init_adb_device(void);

/*
 * The following are private routines.
 */
void	print_single(u_char *);
void	adb_intr(void);
void	adb_intr_II(void);
void	adb_intr_IIsi(void);
void	adb_intr_cuda(void);
int	send_adb_II(u_char *, u_char *, void *, void *, int);
int	send_adb_IIsi(u_char *, u_char *, void *, void *, int);
int	send_adb_cuda(u_char *, u_char *, void *, void *, int);
void	adb_intr_cuda_test(void);
void	adb_handle_unsol(u_char *);
void	adb_op_comprout(void);
void	adb_reinit(void);
int	count_adbs(void);
int	get_ind_adb_info(ADBDataBlock *, int);
int	get_adb_info(ADBDataBlock *, int);
int	set_adb_info(ADBSetInfoBlock *, int);
void	adb_setup_hw_type(void);
int	adb_op(Ptr, Ptr, Ptr, short);
void	adb_handle_unsol(u_char *);
int	adb_op_sync(Ptr, Ptr, Ptr, short);
void	adb_read_II(u_char *);
void	adb_cleanup(u_char *);
void	adb_cleanup_IIsi(u_char *);
void	adb_comp_exec(void);
int	adb_cmd_result(u_char *);
int	adb_cmd_extra(u_char *);
int	adb_guess_next_device(void);
int	adb_prog_switch_enable(void);
int	adb_prog_switch_disable(void);
/* we should create this and it will be the public version */
int	send_adb(u_char *, void *, void *);

/*
 * print_single
 * Diagnostic display routine. Displays the hex values of the
 * specified elements of the u_char. The length of the "string"
 * is in [0].
 */
void
print_single(thestring)
	u_char *thestring;
{
	int x;

	if ((int) (thestring[0]) == 0) {
		printf_intr("nothing returned\n");
		return;
	}
	if (thestring == 0) {
		printf_intr("no data - null pointer\n");
		return;
	}
	if (thestring[0] > 20) {
		printf_intr("ADB: ACK > 20 no way!\n");
		thestring[0] = 20;
	}
	printf_intr("(length=0x%x):", thestring[0]);
	for (x = 0; x < thestring[0]; x++)
		printf_intr("  0x%02x", thestring[x + 1]);
	printf_intr("\n");
}


/*
 * called when when an adb interrupt happens
 *
 * Cuda version of adb_intr
 * TO DO: do we want to add some zshard calls in here?
 */
void
adb_intr_cuda(void)
{
	int i, ending, len;
	unsigned int s;

	s = splhigh();		/* can't be too careful - might be called */
	/* from a routine, NOT an interrupt */

	ADB_VIA_CLR_INTR();	/* clear interrupt */

	ADB_VIA_INTR_DISABLE();	/* disable ADB interrupt on IIs. */

switch_start:
	switch (adbActionState) {
	case ADB_ACTION_IDLE:
		/* This is an unexpected packet, so grab the first (dummy)
		 * byte, set up the proper vars, and tell the chip we are
		 * starting to receive the packet by setting the TIP bit. */
		adbInputBuffer[1] = ADB_SR();
		ADB_SET_STATE_TIP();
		ADB_SET_SR_INPUT();
		delay(ADB_DELAY);	/* required delay */
#ifdef DEBUG
		printf_intr("idle 0x%02x ", adbInputBuffer[1]);
#endif
		adbInputBuffer[0] = 1;
		adbActionState = ADB_ACTION_IN;
		break;

	case ADB_ACTION_IN:
		adbInputBuffer[++adbInputBuffer[0]] = ADB_SR();
		/* intr off means this is the last byte (end of frame) */
		if (ADB_INTR_IS_OFF)
			ending = 1;
		else
			ending = 0;

		/* if the second byte is 0xff, it's a "dummy" packet */
		if (adbInputBuffer[2] == 0xff)
			ending = 1;

		if (1 == ending) {	/* end of message? */
#ifdef DEBUG
			printf_intr("in end 0x%02x ",
			    adbInputBuffer[adbInputBuffer[0]]);
			print_single(adbInputBuffer);
#endif

			/* Are we waiting AND does this packet match what we
			 * are waiting for AND is it coming from either the
			 * ADB or RTC/PRAM sub-device? This section _should_
			 * recognize all ADB and RTC/PRAM type commands, but
			 * there may be more... NOTE: commands are always at
			 * [4], even for RTC/PRAM commands. */
			if ((adbWaiting == 1) &&
			    (adbInputBuffer[4] == adbWaitingCmd) &&
			    ((adbInputBuffer[2] == 0x00) ||
			    (adbInputBuffer[2] == 0x01))) {

				if (adbBuffer != (long) 0) {
					/* if valid return data pointer */
					/* get return length minus extras */
					len = adbInputBuffer[0] - 4;
					/*
					 * If adb_op is ever made to be called
					 * from a user routine, we should use
					 * a copyout or copyin here to be sure
					 * we're in the correct context
					 */
					for (i = 1; i <= len; i++)
						adbBuffer[i] = adbInputBuffer[4 + i];
					if (len < 0)
						len = 0;
					adbBuffer[0] = len;
				}
				/* call completion routine and clean up */
				adb_comp_exec();
				adbWaitingCmd = 0;
				adbWaiting = 0;
				adbBuffer = (long) 0;
				adbCompRout = (long) 0;
				adbCompData = (long) 0;
			} else {
				/*
				 * This was an unsolicited packet, so
				 * pass the data off to the handler for
				 * this device if we are NOT doing this
				 * during a ADBReInit.
				 * This section IGNORES all data that is not
				 * from the ADB sub-device. That is, not from
				 * RTC or PRAM. Maybe we should fix later,
				 * but do the other devices every send things
				 * without being asked?
				 */
				if (adbStarting == 0)
					if (adbInputBuffer[2] == 0x00)
						adb_handle_unsol(adbInputBuffer);
			}

			/* reset vars and signal the end of this frame */
			adbActionState = ADB_ACTION_IDLE;
			adbInputBuffer[0] = 0;
			ADB_SET_STATE_IDLE_CUDA();

			/*
			 * If there is something waiting to be sent out,
			 * the set everything up and send the first byte.
			 */
			if (adbWriteDelay == 1) {
				delay(ADB_DELAY);	/* required */
				adbSentChars = 0;
				adbActionState = ADB_ACTION_OUT;

/* TO DO: don't we need to set up adbWaiting vars here??? */

				/*
				 * If the interrupt is on, we were too slow
				 * and the chip has already started to send
				 * something to us, so back out of the write
				 * and start a read cycle.
				 */
				if (ADB_INTR_IS_ON) {
					ADB_SET_STATE_IDLE_CUDA();
					ADB_SET_SR_INPUT();
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
				ADB_SET_SR_OUTPUT();
				ADB_SR() = adbOutputBuffer[adbSentChars + 1];
				ADB_SET_STATE_TIP();
			}
		} else {
			ADB_TOGGLE_STATE_ACK_CUDA();
#ifdef DEBUG
			printf_intr("in 0x%02x ",
			    adbInputBuffer[adbInputBuffer[0]]);
#endif
		}
		break;

	case ADB_ACTION_OUT:
		i = ADB_SR();	/* reset SR-intr in IFR */
#ifdef DEBUG
		printf_intr("intr out 0x%02x ", i);
#endif
		ADB_SET_SR_OUTPUT();	/* set shift register for OUT */

		adbSentChars++;
		if (ADB_INTR_IS_ON) {	/* ADB intr low during write */
#ifdef DEBUG
			printf_intr("intr was on ");
#endif
			ADB_SET_STATE_IDLE_CUDA();
			ADB_SET_SR_INPUT();	/* make sure SR is set to IN */
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
			} else {/* no talk, so done */
				adb_comp_exec();	/* call completion
							 * routine */
				adbWaitingCmd = 0;	/* reset "waiting" vars,
							 * just in case */
				adbBuffer = (long) 0;
				adbCompRout = (long) 0;
				adbCompData = (long) 0;
			}

			adbWriteDelay = 0;	/* done writing */
			adbActionState = ADB_ACTION_IDLE;	/* signal bus is idle */
			ADB_SET_STATE_IDLE_CUDA();
#ifdef DEBUG
			printf_intr("write done ");
#endif
		} else {
			ADB_SR() = adbOutputBuffer[adbSentChars + 1];	/* send next byte */
			ADB_TOGGLE_STATE_ACK_CUDA();	/* signal byte ready to
							 * shift */
#ifdef DEBUG
			printf_intr("toggle ");
#endif
		}
		break;

	case ADB_ACTION_NOTREADY:
		printf_intr("adb: not yet initialized\n");
		break;

	default:
		printf_intr("intr: unknown ADB state\n");
	}

	ADB_VIA_INTR_ENABLE();	/* enable ADB interrupt on IIs. */

	splx(s);		/* restore */

	return;
}				/* end adb_intr_IIsi */


int
send_adb_cuda(u_char * in, u_char * buffer, void *compRout, void *data, int
	command)
{
	int i, s, len;

#ifdef DEBUG
	printf_intr("SEND\n");
#endif

	if (adbActionState == ADB_ACTION_NOTREADY)
		return 1;

	s = splhigh();		/* don't interrupt while we are messing with
				 * the ADB */

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

#ifdef DEBUG
	printf_intr("QUEUE\n");
#endif
	if ((long) in == (long) 0) {	/* need to convert? */
		/* don't need to use adb_cmd_extra here because this section
		 * will be called */
		/* ONLY when it is an ADB command (no RTC or PRAM) */
		if ((command & 0x0c) == 0x08)	/* copy addl data ONLY if
						 * doing a listen! */
			len = buffer[0];	/* length of additional data */
		else
			len = 0;/* no additional data */

		adbOutputBuffer[0] = 2 + len;	/* dev. type + command + addl.
						 * data */
		adbOutputBuffer[1] = 0x00;	/* mark as an ADB command */
		adbOutputBuffer[2] = (u_char) command;	/* load command */

		for (i = 1; i <= len; i++)	/* copy additional output
						 * data, if any */
			adbOutputBuffer[2 + i] = buffer[i];
	} else
		for (i = 0; i <= (adbOutputBuffer[0] + 1); i++)
			adbOutputBuffer[i] = in[i];

	adbSentChars = 0;	/* nothing sent yet */
	adbBuffer = buffer;	/* save buffer to know where to save result */
	adbCompRout = compRout;	/* save completion routine pointer */
	adbCompData = data;	/* save completion routine data pointer */
	adbWaitingCmd = adbOutputBuffer[2];	/* save wait command */

	if (adbWriteDelay != 1) {	/* start command now? */
#ifdef DEBUG
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

	if (0x0100 <= (s & 0x0700))	/* were VIA1 interrupts blocked ? */
		/* poll until byte done */
		while ((adbActionState != ADB_ACTION_IDLE) || (ADB_INTR_IS_ON)
		    || (adbWaiting == 1))
			if (ADB_SR_INTR_IS_ON)	/* wait for "interrupt" */
				adb_intr_cuda();	/* go process
							 * "interrupt" */

	return 0;
}				/* send_adb_cuda */


/* TO DO: add one or two zshard calls in here */
void
adb_intr_II(void)
{
	int i, len, intr_on = 0;
	int send = 0, do_srq = 0;
	unsigned int s;

	s = splhigh();		/* can't be too careful - might be called */
	/* from a routine, NOT an interrupt */

	ADB_VIA_CLR_INTR();	/* clear interrupt */

	ADB_VIA_INTR_DISABLE();	/* disable ADB interrupt on IIs. */

/*if (ADB_INTR_IS_ON)*/
/*	printf_intr("INTR ON ");*/
	if (ADB_INTR_IS_ON)
		intr_on = 1;	/* save for later */

	switch (adbActionState) {
	case ADB_ACTION_IDLE:
		if (!intr_on) {
			/* printf_intr("FAKE DROPPED \n"); */
			/* printf_intr(" XX "); */
			i = ADB_SR();
			break;
		}
		adbNextEnd = 0;
		/* printf_intr("idle "); */
		adbInputBuffer[0] = 1;
		adbInputBuffer[1] = ADB_SR();	/* get first byte */
		/* printf_intr("0x%02x ", adbInputBuffer[1]); */
		ADB_SET_SR_INPUT();	/* make sure SR is set to IN */
		adbActionState = ADB_ACTION_IN;	/* set next state */
		ADB_SET_STATE_EVEN();	/* set bus state to even */
		adbBusState = ADB_BUS_EVEN;
		break;

	case ADB_ACTION_IN:
		adbInputBuffer[++adbInputBuffer[0]] = ADB_SR();	/* get byte */
		/* printf_intr("in 0x%02x ",
		 * adbInputBuffer[adbInputBuffer[0]]); */
		ADB_SET_SR_INPUT();	/* make sure SR is set to IN */

		/*
		 * Check for an unsolicited Service Request (SRQ).
		 * An empty SRQ packet NEVER ends, so we must manually
		 * check for the following condition.
		 */
		if (adbInputBuffer[0] == 4 && adbInputBuffer[2] == 0xff &&
		    adbInputBuffer[3] == 0xff && adbInputBuffer[4] == 0xff &&
		    intr_on && !adbNextEnd)
			do_srq = 1;

		if (adbNextEnd == 1) {	/* process last byte of packet */
			adbNextEnd = 0;
			/* printf_intr("done: "); */

			/* If the following conditions are true (4 byte
			 * message, last 3 bytes are 0xff) then we basically
			 * got a "no response" from the ADB chip, so change
			 * the message to an empty one. We also clear intr_on
			 * to stop the SRQ send later on because these packets
			 * normally have the SRQ bit set even when there is
			 * NOT a pending SRQ. */
			if (adbInputBuffer[0] == 4 && adbInputBuffer[2] == 0xff &&
			    adbInputBuffer[3] == 0xff && adbInputBuffer[4] == 0xff) {
				/* printf_intr("NO RESP "); */
				intr_on = 0;
				adbInputBuffer[0] = 0;
			}
			adbLastDevice = (adbInputBuffer[1] & 0xf0) >> 4;

			if ((!adbWaiting || adbPolling)
			    && (adbInputBuffer[0] != 0)) {
				/* unsolicided - ignore if starting */
				if (!adbStarting)
					adb_handle_unsol(adbInputBuffer);
			} else
				if (!adbPolling) {	/* someone asked for it */
					/* printf_intr("SOL: "); */
					/* print_single(adbInputBuffer); */
					if (adbBuffer != (long) 0) {	/* if valid return data
									 * pointer */
						/* get return length minus
						 * extras */
						len = adbInputBuffer[0] - 1;

						/* if adb_op is ever made to
						 * be called from a user
						 * routine, we should use a
						 * copyout or copyin here to
						 * be sure we're in the
						 * correct context. */
						for (i = 1; i <= len; i++)
							adbBuffer[i] = adbInputBuffer[i + 1];
						if (len < 0)
							len = 0;
						adbBuffer[0] = len;
					}
					adb_comp_exec();
				}
			adbWaiting = 0;
			adbPolling = 0;
			adbInputBuffer[0] = 0;
			adbBuffer = (long) 0;
			adbCompRout = (long) 0;
			adbCompData = (long) 0;
			/*
			 * Since we are done, check whether there is any data
			 * waiting to do out. If so, start the sending the data.
			 */
			if (adbOutQueueHasData == 1) {
				/* printf_intr("XXX: DOING OUT QUEUE\n"); */
				/* copy over data */
				for (i = 0; i <= (adbOutQueue.outBuf[0] + 1); i++)
					adbOutputBuffer[i] = adbOutQueue.outBuf[i];
				adbBuffer = adbOutQueue.saveBuf;	/* user data area */
				adbCompRout = adbOutQueue.compRout;	/* completion routine */
				adbCompData = adbOutQueue.data;	/* comp. rout. data */
				adbOutQueueHasData = 0;	/* currently processing
							 * "queue" entry */
				adbPolling = 0;
				send = 1;
				/* if intr_on is true, then it's a SRQ so poll
				 * other devices. */
			} else
				if (intr_on) {
					/* printf_intr("starting POLL "); */
					do_srq = 1;
					adbPolling = 1;
				} else
					if ((adbInputBuffer[1] & 0x0f) != 0x0c) {
						/* printf_intr("xC HACK "); */
						adbPolling = 1;
						send = 1;
						adbOutputBuffer[0] = 1;
						adbOutputBuffer[1] = (adbInputBuffer[1] & 0xf0) | 0x0c;
					} else {
						/* printf_intr("ending "); */
						adbBusState = ADB_BUS_IDLE;
						adbActionState = ADB_ACTION_IDLE;
						ADB_SET_STATE_IDLE_II();
						break;
					}
		}
		/*
		 * If do_srq is true then something above determined that
		 * the message has ended and some device is sending a
		 * service request. So we need to determine the next device
		 * and send a poll to it. (If the device we send to isn't the
		 * one that sent the SRQ, that ok as it will be caught
		 * the next time though.)
		 */
		if (do_srq) {
			/* printf_intr("SRQ! "); */
			adbPolling = 1;
			adb_guess_next_device();
			adbOutputBuffer[0] = 1;
			adbOutputBuffer[1] = ((adbLastDevice & 0x0f) << 4) | 0x0c;
			send = 1;
		}
		/*
		 * If send is true then something above determined that
		 * the message has ended and we need to start sending out
		 * a new message immediately. This could be because there
		 * is data waiting to go out or because an SRQ was seen.
		 */
		if (send) {
			adbNextEnd = 0;
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
		if (!intr_on)	/* if adb intr. on then the */
			adbNextEnd = 1;	/* NEXT byte is the last */

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
		adbNextEnd = 0;
		if (!adbPolling)
			adbWaiting = 1;	/* not unsolicited */
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
			/* printf_intr("talk out 0x%02x ", i); */
			break;
		}
		/* If it's not a TALK, check whether all data has been sent.
		 * If so, call the completion routine and clean up. If not,
		 * advance to the next state. */
		/* printf_intr("non-talk out 0x%0x ", i); */
		ADB_SET_SR_OUTPUT();
		if (adbOutputBuffer[0] == adbSentChars) {	/* check for done */
			/* printf_intr("done \n"); */
			adb_comp_exec();
			adbBuffer = (long) 0;
			adbCompRout = (long) 0;
			adbCompData = (long) 0;
			if (adbOutQueueHasData == 1) {
				/* copy over data */
				for (i = 0; i <= (adbOutQueue.outBuf[0] + 1); i++)
					adbOutputBuffer[i] = adbOutQueue.outBuf[i];
				adbBuffer = adbOutQueue.saveBuf;	/* user data area */
				adbCompRout = adbOutQueue.compRout;	/* completion routine */
				adbCompData = adbOutQueue.data;	/* comp. rout. data */
				adbOutQueueHasData = 0;	/* currently processing
							 * "queue" entry */
				adbPolling = 0;
			} else {
				adbOutputBuffer[0] = 1;
				adbOutputBuffer[1] = (adbOutputBuffer[1] & 0xf0) | 0x0c;
				adbPolling = 1;	/* non-user poll */
			}
			adbNextEnd = 0;
			adbSentChars = 0;	/* nothing sent yet */
			adbActionState = ADB_ACTION_OUT;	/* set next state */
			ADB_SET_SR_OUTPUT();	/* set shift register for OUT */
			ADB_SR() = adbOutputBuffer[1];	/* load byte for output */
			adbBusState = ADB_BUS_CMD;	/* set bus to cmd state */
			ADB_SET_STATE_CMD();	/* tell ADB that we want to
						 * send */
			break;
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
			printf_intr("strange state!!! (0x%x)\n", adbBusState);
			break;
		}
		break;

	default:
		printf_intr("adb: unknown ADB state (during intr)\n");
	}

	ADB_VIA_INTR_ENABLE();	/* enable ADB interrupt on IIs. */

	splx(s);		/* restore */

	return;

}


/*
 * send_adb version for II series machines
 */
int
send_adb_II(u_char * in, u_char * buffer, void *compRout, void *data, int command)
{
	int i, s, len;

	if (adbActionState == ADB_ACTION_NOTREADY)	/* return if ADB not
							 * available */
		return 1;

	s = splhigh();		/* don't interrupt while we are messing with
				 * the ADB */

	if (0 != adbOutQueueHasData) {	/* right now, "has data" means "full" */
		splx(s);	/* sorry, try again later */
		return 1;
	}
	if ((long) in == (long) 0) {	/* need to convert? */
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
		adbOutQueue.outBuf[1] = (u_char) command;	/* load command */

		for (i = 1; i <= len; i++)	/* copy additional output
						 * data, if any */
			adbOutQueue.outBuf[1 + i] = buffer[i];
	} else
		/* if data ready, just copy over */
		for (i = 0; i <= (adbOutQueue.outBuf[0] + 1); i++)
			adbOutQueue.outBuf[i] = in[i];

	adbOutQueue.saveBuf = buffer;	/* save buffer to know where to save
					 * result */
	adbOutQueue.compRout = compRout;	/* save completion routine
						 * pointer */
	adbOutQueue.data = data;/* save completion routine data pointer */

	if ((adbActionState == ADB_ACTION_IDLE) &&	/* is ADB available? */
	    (ADB_INTR_IS_OFF) &&/* and no incoming interrupts? */
	    (adbPolling == 0)) {/* and we are not currently polling */
		/* then start command now */
		for (i = 0; i <= (adbOutQueue.outBuf[0] + 1); i++)	/* copy over data */
			adbOutputBuffer[i] = adbOutQueue.outBuf[i];

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

	if (0x0100 <= (s & 0x0700))	/* were VIA1 interrupts blocked ? */
		/* poll until message done */
		while ((adbActionState != ADB_ACTION_IDLE) || (ADB_INTR_IS_ON)
		    || (adbWaiting == 1) || (adbPolling == 1))
			if (ADB_SR_INTR_IS_ON)	/* wait for "interrupt" */
				adb_intr_II();	/* go process "interrupt" */

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
		/* start polling EVERY device, since we can't be sure there is
		 * anything in the device table yet */
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
				dummy = 2;
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
void
adb_intr(void)
{
	switch (adbHardware) {
		case ADB_HW_II:
		adb_intr_II();
		break;

	case ADB_HW_IISI:
		adb_intr_IIsi();
		break;

	case ADB_HW_PB:
		break;

	case ADB_HW_CUDA:
		adb_intr_cuda();
		break;

	case ADB_HW_UNKNOWN:
		break;
	}
}


/*
 * called when when an adb interrupt happens
 *
 * IIsi version of adb_intr
 *
 */
void
adb_intr_IIsi(void)
{
	int i, ending, len;
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
		(void)intr_dispatch(0x70);
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
		(void)intr_dispatch(0x70);

		if (1 == ending) {	/* end of message? */
			ADB_SET_STATE_INACTIVE();	/* signal end of frame */
			/* this section _should_ handle all ADB and RTC/PRAM
			 * type commands, */
			/* but there may be more... */
			/* note: commands are always at [4], even for rtc/pram
			 * commands */
			if ((adbWaiting == 1) &&	/* are we waiting AND */
			    (adbInputBuffer[4] == adbWaitingCmd) &&	/* the cmd we sent AND */
			    ((adbInputBuffer[2] == 0x00) ||	/* it's from the ADB
								 * device OR */
				(adbInputBuffer[2] == 0x01))) {	/* it's from the
								 * PRAM/RTC device */

				/* is this data we are waiting for? */
				if (adbBuffer != (long) 0) {	/* if valid return data
								 * pointer */
					/* get return length minus extras */
					len = adbInputBuffer[0] - 4;
					/* if adb_op is ever made to be called
					 * from a user routine, we should use
					 * a copyout or copyin here to be sure
					 * we're in the correct context */
					for (i = 1; i <= len; i++)
						adbBuffer[i] = adbInputBuffer[4 + i];
					if (len < 0)
						len = 0;
					adbBuffer[0] = len;
				}
				adb_comp_exec();	/* call completion
							 * routine */

				adbWaitingCmd = 0;	/* reset "waiting" vars */
				adbWaiting = 0;
				adbBuffer = (long) 0;
				adbCompRout = (long) 0;
				adbCompData = (long) 0;
			} else {
				/* pass the data off to the handler */
				/* This section IGNORES all data that is not
				 * from the ADB sub-device. That is, not from
				 * rtc or pram. Maybe we should fix later,
				 * but do the other devices every send things
				 * without being asked? */
				if (adbStarting == 0)	/* ignore if during
							 * adbreinit */
					if (adbInputBuffer[2] == 0x00)
						adb_handle_unsol(adbInputBuffer);
			}

			adbActionState = ADB_ACTION_IDLE;
			adbInputBuffer[0] = 0;	/* reset length */

			if (adbWriteDelay == 1) {	/* were we waiting to
							 * write? */
				adbSentChars = 0;	/* nothing sent yet */
				adbActionState = ADB_ACTION_OUT;	/* set next state */

				delay(ADB_DELAY);	/* delay */
				(void)intr_dispatch(0x70);

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
			(void)intr_dispatch(0x70);
			goto switch_start;	/* process next state right
						 * now */
			break;
		}
		delay(ADB_DELAY);	/* required delay */
		(void)intr_dispatch(0x70);

		if (adbOutputBuffer[0] == adbSentChars) {	/* check for done */
			if (0 == adb_cmd_result(adbOutputBuffer)) {	/* do we expect data
									 * back? */
				adbWaiting = 1;	/* signal waiting for return */
				adbWaitingCmd = adbOutputBuffer[2];	/* save waiting command */
			} else {/* no talk, so done */
				adb_comp_exec();	/* call completion
							 * routine */
				adbWaitingCmd = 0;	/* reset "waiting" vars,
							 * just in case */
				adbBuffer = (long) 0;
				adbCompRout = (long) 0;
				adbCompData = (long) 0;
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
		printf_intr("adb: not yet initialized\n");
		break;

	default:
		printf_intr("intr: unknown ADB state\n");
	}

	ADB_VIA_INTR_ENABLE();	/* enable ADB interrupt on IIs. */

	splx(s);		/* restore */

	return;
}				/* end adb_intr_IIsi */


/*****************************************************************************
 * if the device is currently busy, and there is no data waiting to go out, then
 * the data is "queued" in the outgoing buffer. If we are already waiting, then
 * we return.
 * in: if (in==0) then the command string is built from command and buffer
 *     if (in!=0) then in is used as the command string
 * buffer: additional data to be sent (used only if in==0)
 *         this is also where return data is stored
 * compRout: the completion routine that is called when then return value
 *	     is received (if a return value is expected)
 * data: a data pointer that can be used by the completion routine
 * command: an ADB command to be sent (used only if in==0)
 *
 */
int
send_adb_IIsi(u_char * in, u_char * buffer, void *compRout, void *data, int
	command)
{
	int i, s, len;

	if (adbActionState == ADB_ACTION_NOTREADY)
		return 1;

	s = splhigh();		/* don't interrupt while we are messing with
				 * the ADB */

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

	if ((long) in == (long) 0) {	/* need to convert? */
		/* don't need to use adb_cmd_extra here because this section
		 * will be called */
		/* ONLY when it is an ADB command (no RTC or PRAM) */
		if ((command & 0x0c) == 0x08)	/* copy addl data ONLY if
						 * doing a listen! */
			len = buffer[0];	/* length of additional data */
		else
			len = 0;/* no additional data */

		adbOutputBuffer[0] = 2 + len;	/* dev. type + command + addl.
						 * data */
		adbOutputBuffer[1] = 0x00;	/* mark as an ADB command */
		adbOutputBuffer[2] = (u_char) command;	/* load command */

		for (i = 1; i <= len; i++)	/* copy additional output
						 * data, if any */
			adbOutputBuffer[2 + i] = buffer[i];
	} else
		for (i = 0; i <= (adbOutputBuffer[0] + 1); i++)
			adbOutputBuffer[i] = in[i];

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

	if (0x0100 <= (s & 0x0700))	/* were VIA1 interrupts blocked ? */
		/* poll until byte done */
		while ((adbActionState != ADB_ACTION_IDLE) || (ADB_INTR_IS_ON)
		    || (adbWaiting == 1))
			if (ADB_SR_INTR_IS_ON)	/* wait for "interrupt" */
				adb_intr_IIsi();	/* go process
							 * "interrupt" */

	return 0;
}				/* send_adb_IIsi */


/*
 * adb_comp_exec
 * This is a general routine that calls the completion routine if there is one.
 */
void
adb_comp_exec(void)
{
	if ((long) 0 != adbCompRout)	/* don't call if empty return location */
#if defined(__NetBSD__) || defined(__OpenBSD__)
		asm("
		    movml #0xffff, sp@-		| save all registers
		    movl %0, a2 		| adbCompData
		    movl %1, a1 		| adbCompRout
		    movl %2, a0 		| adbBuffer
		    movl %3, d0 		| adbWaitingCmd
		    jbsr a1@ 			| go call the routine
		    movml sp@+, #0xffff		| restore all registers"
		    :
		    :"g"(adbCompData), "g"(adbCompRout),
		     "g"(adbBuffer), "g"(adbWaitingCmd)
		    :"d0", "a0", "a1", "a2");
#else					/* for macos based testing */
		asm {
			movem.l a0/a1/a2/d0, -(a7)
			move.l adbCompData, a2
			move.l adbCompRout, a1
			move.l adbBuffer, a0
			move.w adbWaitingCmd, d0
			jsr(a1)
			movem.l(a7) +, d0/a2/a1/a0
		}
#endif
}


/*
 * This routine handles what needs to be done after an unsolicited
 * message is read from the ADB device.  'in' points to the raw
 * data received from the device, including device number
 * (on IIsi) and result code.
 *
 * Note that the service (completion) routine for an unsolicited
 * message is whatever is set in the ADB device table. This is
 * different than for a device responding to a specific request,
 * where the completion routine is defined by the caller.
 */
void
adb_handle_unsol(u_char * in)
{
	int i, cmd = 0;
	u_char data[MAX_ADB_MSG_LENGTH];
	u_char *buffer = 0;
	ADBDataBlock block;

	/* make local copy so we don't destroy the real one - it may be needed
	 * later. */
	for (i = 0; i <= (in[0] + 1); i++)
		data[i] = in[i];

	switch (adbHardware) {
	case ADB_HW_II:
		/* adjust the "length" byte */
		cmd = data[1];
		if (data[0] < 2)
			data[1] = 0;
		else
			data[1] = data[0] - 1;

		buffer = (data + 1);
		break;

	case ADB_HW_IISI:
	case ADB_HW_CUDA:
		/* only handles ADB for now */
		if (0 != *(data + 2))
			return;

		/* adjust the "length" byte */
		cmd = data[4];
		if (data[0] < 5)
			data[4] = 0;
		else
			data[4] = data[0] - 4;

		buffer = (data + 4);
		break;

	case ADB_HW_PB:
		return;		/* how does PM handle "unsolicited" messages? */

	case ADB_HW_UNKNOWN:
		return;
	}

	if (-1 == get_adb_info(&block, ((cmd & 0xf0) >> 4)))
		return;

	/* call default completion routine if it's valid */
	/* TO DO: This section of code is somewhat redundant with
	 * adb_comp_exec (above). Some day we may want to generalize it and
	 * make it a single function. */
	if ((long) 0 != (long) block.dbServiceRtPtr) {
#if defined(__NetBSD__) || defined(__OpenBSD__)
		asm("
		    movml #0xffff, sp@-		| save all registers
		    movl %0, a2 		| block.dbDataAreaAddr
		    movl %1, a1 		| block.dbServiceRtPtr
		    movl %2, a0 		| buffer
		    movl %3, d0 		| cmd
		    jbsr a1@ 			| go call the routine
		    movml sp@+, #0xffff		| restore all registers"
		    :
		    : "g"(block.dbDataAreaAddr),
		      "g"(block.dbServiceRtPtr), "g"(buffer), "g"(cmd)
		    : "d0", "a0", "a1", "a2");
#else					/* for macos based testing */
		asm
		{
			movem.l a0/a1/a2/d0, -(a7)
			move.l block.dbDataAreaAddr, a2
			move.l block.dbServiceRtPtr, a1
			move.l buffer, a0
			move.w cmd, d0
			jsr(a1)
			movem.l(a7) +, d0/a2/a1/a0
		}
#endif
	}
	return;
}


/*
 * This is my version of the ADBOp routine. It mainly just calls the hardware-specific
 * routine.
 *
 *   data 	: pointer to data area to be used by compRout
 *   compRout	: completion routine
 *   buffer	: for LISTEN: points to data to send - MAX 8 data bytes,
 *		  byte 0 = # of bytes
 *		: for TALK: points to place to save return data
 *   command	: the adb command to send
 *   result     : 0 = success
 *              : -1 = could not complete
 */
int
adb_op(Ptr buffer, Ptr compRout, Ptr data, short command)
{
	int result;

	switch (adbHardware) {
	case ADB_HW_II:
		result = send_adb_II((u_char *) 0,
		    (u_char *) buffer, (void *) compRout,
		    (void *) data, (int) command);
		if (result == 0)
			return 0;
		else
			return -1;
		break;

	case ADB_HW_IISI:
		result = send_adb_IIsi((u_char *) 0,
		    (u_char *) buffer, (void *) compRout,
		    (void *) data, (int) command);
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
		result = send_adb_cuda((u_char *) 0,
		    (u_char *) buffer, (void *) compRout,
		    (void *) data, (int) command);
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
 * adb_cleanup
 * This routine simply calls the appropriate version of the adb_cleanup routine.
 */
void
adb_cleanup(u_char * in)
{
	volatile int i;

	switch (adbHardware) {
	case ADB_HW_II:
		ADB_VIA_CLR_INTR();	/* clear interrupt */
		break;

	case ADB_HW_IISI:
		/* get those pesky clock ticks we missed while booting */
		adb_cleanup_IIsi(in);
		break;

	case ADB_HW_PB:
		/*
		 * XXX -  really PM_VIA_CLR_INTR - should we put it in
		 * pm_direct.h?
		 */
		via_reg(VIA1, vIFR) = 0x90;	/* clear interrupt */
		break;

	case ADB_HW_CUDA:
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
		return;
	}
}


/*
 * adb_cleanup_IIsi
 * This is sort of a "read" routine that forces the adb hardware through a read cycle
 * if there is something waiting. This helps "clean up" any commands that may have gotten
 * stuck or stopped during the boot process.
 *
 */
void
adb_cleanup_IIsi(u_char * buffer)
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
			/* poll for ADB interrupt and watch for timeout */
			/* if time out, keep going in hopes of not hanging the
			 * ADB chip - I think */
			my_time = ADB_DELAY * 5;
			while ((ADB_SR_INTR_IS_OFF) && (my_time-- > 0))
				dummy = via_reg(VIA1, vBufB);

			buffer[i++] = ADB_SR();	/* reset interrupt flag by
						 * reading vSR */
			/* perhaps put in a check here that ignores all data
			 * after the first MAX_ADB_MSG_LENGTH bytes ??? */
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
}				/* adb_cleanup_IIsi */



/*
 * adb_reinit sets up the adb stuff
 *
 */
void
adb_reinit(void)
{
	u_char send_string[MAX_ADB_MSG_LENGTH];
	int s = 0;
	volatile int i, x;
	int command;
	int result;
	int saveptr;		/* point to next free relocation address */
	int device;
	int nonewtimes;		/* times thru loop w/o any new devices */
	ADBDataBlock data;	/* temp. holder for getting device info */

	/* Make sure we are not interrupted while building the table. */
	if (adbHardware != ADB_HW_PB)	/* ints must be on for PB? */
		s = splhigh();

	ADBNumDevices = 0;	/* no devices yet */

	/* Let intr routines know we are running reinit */
	adbStarting = 1;

	/* Initialize the ADB table.  For now, we'll always use the same table
	 * that is defined at the beginning of this file - no mallocs. */
	for (i = 0; i < 16; i++)
		ADBDevTable[i].devType = 0;

	adb_setup_hw_type();	/* setup hardware type */

	/* Set up all the VIA bits we need to do the ADB stuff. */
	switch (adbHardware) {
	case ADB_HW_II:
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
		break;

	case ADB_HW_IISI:
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
		break;

	case ADB_HW_PB:
		break;		/* there has to be more than this? */

	case ADB_HW_CUDA:
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
		break;

	case ADB_HW_UNKNOWN:	/* if type unknown then skip out */
	default:
		via_reg(VIA1, vIER) = 0x04;	/* turn interrupts off - TO
						 * DO: turn PB ints off? */
		splx(s);
		return;
	}

	/*
	 * Clear out any "leftover" commands.  Remember that up until this
	 * point, the interrupt routine will be either off or it should be
	 * able to ignore inputs until the device table is built.
	 */
	for (i = 0; i < 30; i++) {
		delay(ADB_DELAY);
		adb_cleanup(send_string);
		printf_intr("adb: cleanup: ");
		print_single(send_string);
		delay(ADB_DELAY);
		if (ADB_INTR_IS_OFF)
			break;
	}

	/* send an ADB reset first */
	adb_op_sync((Ptr) 0, (Ptr) 0, (Ptr) 0, (short) 0x00);

	/* Probe for ADB devices. Probe devices 1-15 quickly to determine
	 * which device addresses are in use and which are free. For each
	 * address that is in use, move the device at that address to a higher
	 * free address. Continue doing this at that address until no device
	 * responds at that address. Then move the last device that was moved
	 * back to the original address. Do this for the remaining addresses
	 * that we determined were in use.
	 * 
	 * When finished, do this entire process over again with the updated list
	 * of in use addresses. Do this until no new devices have been found
	 * in 20 passes though the in use address list. (This probably seems
	 * long and complicated, but it's the best way to detect multiple
	 * devices at the same address - sometimes it takes a couple of tries
	 * before the collision is detected.) */

	/* initial scan through the devices */
	for (i = 1; i < 16; i++) {
		command = (int) (0x0f | ((int) (i & 0x000f) << 4));	/* talk R3 */
		result = adb_op_sync((Ptr) send_string, (Ptr) 0, (Ptr) 0, (short) command);
		if (0x00 != send_string[0]) {	/* anything come back ?? */
			ADBDevTable[++ADBNumDevices].devType = (u_char) send_string[2];
			ADBDevTable[ADBNumDevices].origAddr = i;
			ADBDevTable[ADBNumDevices].currentAddr = i;
			ADBDevTable[ADBNumDevices].DataAreaAddr = (long) 0;
			ADBDevTable[ADBNumDevices].ServiceRtPtr = (void *) 0;
			/* printf_intr("initial device found (at index %i)\n",
			 * ADBNumDevices); */
			pm_check_adb_devices(i);	/* tell pm driver device
							 * is here */
		}
	}

	/* find highest unused address */
	for (saveptr = 15; saveptr > 0; saveptr--)
		if (-1 == get_adb_info(&data, saveptr))
			break;

	if (saveptr == 0)	/* no free addresses??? */
		saveptr = 15;

	/* printf_intr("first free is: 0x%02x\n", saveptr); */
	/* printf_intr("devices: %i\n", ADBNumDevices); */

	nonewtimes = 0;		/* no loops w/o new devices */
	while (nonewtimes++ < 11) {
		for (i = 1; i <= ADBNumDevices; i++) {
			device = ADBDevTable[i].currentAddr;
			/* printf_intr("moving device 0x%02x to 0x%02x (index
			 * 0x%02x)  ", device, saveptr, i); */

			/* send TALK R3 to address */
			command = (int) (0x0f | ((int) (device & 0x000f) << 4));
			adb_op_sync((Ptr) send_string, (Ptr) 0, (Ptr) 0, (short) command);

			/* move device to higher address */
			command = (int) (0x0b | ((int) (device & 0x000f) << 4));
			send_string[0] = 2;
			send_string[1] = (u_char) (saveptr | 0x60);
			send_string[2] = 0xfe;
			adb_op_sync((Ptr) send_string, (Ptr) 0, (Ptr) 0, (short) command);

			/* send TALK R3 - anything at old address? */
			command = (int) (0x0f | ((int) (device & 0x000f) << 4));
			result = adb_op_sync((Ptr) send_string, (Ptr) 0, (Ptr) 0, (short) command);
			if (send_string[0] != 0) {
				/* new device found */
				/* update data for previously moved device */
				ADBDevTable[i].currentAddr = saveptr;
				/* printf_intr("old device at index %i\n",i); */
				/* add new device in table */
				/* printf_intr("new device found\n"); */
				ADBDevTable[++ADBNumDevices].devType = (u_char) send_string[2];
				ADBDevTable[ADBNumDevices].origAddr = device;
				ADBDevTable[ADBNumDevices].currentAddr = device;
				/* These will be set correctly in adbsys.c */
				/* Until then, unsol. data will be ignored. */
				ADBDevTable[ADBNumDevices].DataAreaAddr = (long) 0;
				ADBDevTable[ADBNumDevices].ServiceRtPtr = (void *) 0;
				/* find next unused address */
				for (x = saveptr; x > 0; x--)
					if (-1 == get_adb_info(&data, x)) {
						saveptr = x;
						break;
					}
				/* printf_intr("new free is 0x%02x\n",
				 * saveptr); */
				nonewtimes = 0;
				/* tell pm driver device is here */
				pm_check_adb_devices(device);
			} else {
				/* printf_intr("moving back...\n"); */
				/* move old device back */
				command = (int) (0x0b | ((int) (saveptr & 0x000f) << 4));
				send_string[0] = 2;
				send_string[1] = (u_char) (device | 0x60);
				send_string[2] = 0xfe;
				adb_op_sync((Ptr) send_string, (Ptr) 0, (Ptr) 0, (short) command);
			}
		}
	}

#ifdef DEBUG
	for (i = 1; i <= ADBNumDevices; i++) {
		x = get_ind_adb_info(&data, i);
		if (x != -1)
			printf_intr("index 0x%x, addr 0x%x, type 0x%x\n", i, x, data.devType);

	}
#endif

	adb_prog_switch_enable();	/* enable the programmer's switch, if
					 * we have one */

	if (0 == ADBNumDevices)	/* tell user if no devices found */
		printf_intr("adb: no devices found\n");

	adbStarting = 0;	/* not starting anymore */
	printf_intr("adb: ADBReInit complete\n");

	if (adbHardware != ADB_HW_PB)	/* ints must be on for PB? */
		splx(s);
	return;
}


/* adb_cmd_result
 * This routine lets the caller know whether the specified adb command string should
 * expect a returned result, such as a TALK command.
 * returns: 0 if a result should be expected
 *          1 if a result should NOT be expected
 */
int
adb_cmd_result(u_char * in)
{
	switch (adbHardware) {
		case ADB_HW_II:
		/* was it an ADB talk command? */
		if ((in[1] & 0x0c) == 0x0c)
			return 0;
		else
			return 1;
		break;

	case ADB_HW_IISI:
	case ADB_HW_CUDA:
		/* was is an ADB talk command? */
		if ((in[1] == 0x00) && ((in[2] & 0x0c) == 0x0c))
			return 0;
		/* was is an RTC/PRAM read date/time? */
		else
			if ((in[1] == 0x01) && (in[2] == 0x03))
				return 0;
			else
				return 1;
		break;

	case ADB_HW_PB:
		return 1;
		break;

	case ADB_HW_UNKNOWN:
	default:
		return 1;
	}
}


/* adb_cmd_extra
 * This routine lets the caller know whether the specified adb command string may have
 * extra data appended to the end of it, such as a LISTEN command.
 * returns: 0 if extra data is allowed
 *          1 if extra data is NOT allowed
 */
int
adb_cmd_extra(u_char * in)
{
	switch (adbHardware) {
		case ADB_HW_II:
		if ((in[1] & 0x0c) == 0x08)	/* was it a listen command? */
			return 0;
		else
			return 1;
		break;

	case ADB_HW_IISI:
	case ADB_HW_CUDA:
		/* TO DO: support needs to be added to recognize RTC and PRAM
		 * commands */
		if ((in[2] & 0x0c) == 0x08)	/* was it a listen command? */
			return 0;
		else		/* add others later */
			return 1;
		break;

	case ADB_HW_PB:
		return 1;
		break;

	case ADB_HW_UNKNOWN:
	default:
		return 1;
	}
}


/* adb_op_sync
 * This routine does exactly what the adb_op routine does, except that after the
 * adb_op is called, it waits until the return value is present before returning
 */
int
adb_op_sync(Ptr buffer, Ptr compRout, Ptr data, short command)
{
	int result;
	volatile int flag = 0;

	result = adb_op(buffer, (void *) adb_op_comprout,
	    (void *) &flag, command);	/* send command */
	if (result == 0) {	/* send ok? */
		while (0 == flag);	/* wait for compl. routine */
		return 0;
	} else
		return result;
}


/* adb_op_comprout
 * This function is used by the adb_op_sync routine so it knows when the function is
 * done.
 */
void 
adb_op_comprout(void)
{
#if defined(__NetBSD__) || defined(__OpenBSD__)
	asm("movw	#1,a2@			| update flag value");
#else				/* for macos based testing */
	asm {
		move.w #1,(a2) }		/* update flag value */
#endif
}

void 
adb_setup_hw_type(void)
{
	long response;

	response = mac68k_machine.machineid;

	switch (response) {
	case 6:		/* II */
	case 7:		/* IIx */
	case 8:		/* IIcx */
	case 9:		/* SE/30 */
	case 11:	/* IIci */
	case 22:	/* Quadra 700 */
	case 30:	/* Centris 650 */
	case 35:	/* Quadra 800 */
	case 36:	/* Quadra 650 */
	case 52:	/* Centris 610 */
	case 53:	/* Quadra 610 */
		adbHardware = ADB_HW_II;
		printf_intr("adb: using II series hardware support\n");
		break;
	case 18:	/* IIsi */
	case 20:	/* Quadra 900 - not sure if IIsi or not */
	case 23:	/* Classic II */
	case 26:	/* Quadra 950 - not sure if IIsi or not */
	case 27:	/* LC III, Performa 450 */
	case 37:	/* LC II, Performa 400/405/430 */
	case 44:	/* IIvi */
	case 45:	/* Performa 600 */
	case 48:	/* IIvx */
	case 49:	/* Color Classic - not sure if IIsi or not */
	case 62:	/* Performa 460/465/467 */
	case 83:	/* Color Classic II - not sure if IIsi or not */
		adbHardware = ADB_HW_IISI;
		printf_intr("adb: using IIsi series hardware support\n");
		break;
	case 21:	/* PowerBook 170 */
	case 25:	/* PowerBook 140 */
	case 54:	/* PowerBook 145 */
	case 34:	/* PowerBook 160 */
	case 84:	/* PowerBook 165 */
	case 50:	/* PowerBook 165c */
	case 33:	/* PowerBook 180 */
	case 71:	/* PowerBook 180c */
	case 115:	/* PowerBook 150 */
		adbHardware = ADB_HW_PB;
		pm_setup_adb();
		printf_intr("adb: using PowerBook 100-series hardware support\n");
		break;
	case 29:	/* PowerBook Duo 210 */
	case 32:	/* PowerBook Duo 230 */
	case 38:	/* PowerBook Duo 250 */
	case 72:	/* PowerBook 500 series */
	case 77:	/* PowerBook Duo 270 */
	case 102:	/* PowerBook Duo 280 */
	case 103:	/* PowerBook Duo 280c */
		adbHardware = ADB_HW_PB;
		pm_setup_adb();
		printf_intr("adb: using PowerBook Duo-series and PowerBook 500-series hardware support\n");
		break;
	case 56:	/* LC 520 */
	case 60:	/* Centris 660AV */
	case 78:	/* Quadra 840AV */
	case 80:	/* LC 550, Performa 550 */
	case 89:	/* LC 475, Performa 475/476 */
	case 92:	/* LC 575, Performa 575/577/578 */
	case 94:	/* Quadra 605 */
	case 98:	/* LC 630, Performa 630, Quadra 630 */
		adbHardware = ADB_HW_CUDA;
		printf_intr("adb: using Cuda series hardware support\n");
		break;
	default:
		adbHardware = ADB_HW_UNKNOWN;
		printf_intr("adb: hardware type unknown for this machine\n");
		printf_intr("adb: ADB support is disabled\n");
		break;
	}
}
	
int 
count_adbs(void)
{
	int i;
	int found;

	found = 0;

	for (i = 1; i < 16; i++)
		if (0 != ADBDevTable[i].devType)
			found++;

	return found;
}

int 
get_ind_adb_info(ADBDataBlock * info, int index)
{
	if ((index < 1) || (index > 15))	/* check range 1-15 */
		return (-1);

	/* printf_intr("index 0x%x devType is: 0x%x\n", index,
	    ADBDevTable[index].devType); */
	if (0 == ADBDevTable[index].devType)	/* make sure it's a valid entry */
		return (-1);

	info->devType = ADBDevTable[index].devType;
	info->origADBAddr = ADBDevTable[index].origAddr;
	info->dbServiceRtPtr = (Ptr) ADBDevTable[index].ServiceRtPtr;
	info->dbDataAreaAddr = (Ptr) ADBDevTable[index].DataAreaAddr;

	return (ADBDevTable[index].currentAddr);
}

int 
get_adb_info(ADBDataBlock * info, int adbAddr)
{
	int i;

	if ((adbAddr < 1) || (adbAddr > 15))	/* check range 1-15 */
		return (-1);

	for (i = 1; i < 15; i++)
		if (ADBDevTable[i].currentAddr == adbAddr) {
			info->devType = ADBDevTable[i].devType;
			info->origADBAddr = ADBDevTable[i].origAddr;
			info->dbServiceRtPtr = (Ptr)ADBDevTable[i].ServiceRtPtr;
			info->dbDataAreaAddr = ADBDevTable[i].DataAreaAddr;
			return 0;	/* found */
		}

	return (-1);		/* not found */
}

int 
set_adb_info(ADBSetInfoBlock * info, int adbAddr)
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

#ifndef MRG_ADB
int
mrg_adbintr(void)
{
	adb_intr();
	return 1;	/* mimic mrg_adbintr in macrom.h just in case */
}

int
mrg_pmintr(void)	/* we don't do this yet */
{
	pm_intr();
	return 1;	/* mimic mrg_pmintr in macrom.h just in case */
}
#endif

/* caller should really use machine-independent version: getPramTime */
/* this version does pseudo-adb access only */
int 
adb_read_date_time(unsigned long *time)
{
	u_char output[MAX_ADB_MSG_LENGTH];
	int result;
	volatile int flag = 0;

	switch (adbHardware) {
	case ADB_HW_II:
		return -1;

	case ADB_HW_IISI:
		output[0] = 0x02;	/* 2 byte message */
		output[1] = 0x01;	/* to pram/rtc device */
		output[2] = 0x03;	/* read date/time */
		result = send_adb_IIsi((u_char *) output,
		    (u_char *) output, (void *) adb_op_comprout,
		    (int *) &flag, (int) 0);
		if (result != 0)	/* exit if not sent */
			return -1;

		while (0 == flag)	/* wait for result */
			;

		*time = (long) (*(long *) (output + 1));
		return 0;

	case ADB_HW_PB:
		return -1;

	case ADB_HW_CUDA:
		output[0] = 0x02;	/* 2 byte message */
		output[1] = 0x01;	/* to pram/rtc device */
		output[2] = 0x03;	/* read date/time */
		result = send_adb_cuda((u_char *) output,
		    (u_char *) output, (void *) adb_op_comprout,
		    (void *) &flag, (int) 0);
		if (result != 0)	/* exit if not sent */
			return -1;

		while (0 == flag)	/* wait for result */
			;

		*time = (long) (*(long *) (output + 1));
		return 0;

	case ADB_HW_UNKNOWN:
	default:
		return -1;
	}
}

/* caller should really use machine-independent version: setPramTime */
/* this version does pseudo-adb access only */
int 
adb_set_date_time(unsigned long time)
{
	u_char output[MAX_ADB_MSG_LENGTH];
	int result;
	volatile int flag = 0;

	switch (adbHardware) {
	case ADB_HW_II:
		return -1;

	case ADB_HW_IISI:
		output[0] = 0x06;	/* 6 byte message */
		output[1] = 0x01;	/* to pram/rtc device */
		output[2] = 0x09;	/* set date/time */
		output[3] = (u_char) (time >> 24);
		output[4] = (u_char) (time >> 16);
		output[5] = (u_char) (time >> 8);
		output[6] = (u_char) (time);
		result = send_adb_IIsi((u_char *) output,
		    (u_char *) 0, (void *) adb_op_comprout,
		    (void *) &flag, (int) 0);
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
		output[3] = (u_char) (time >> 24);
		output[4] = (u_char) (time >> 16);
		output[5] = (u_char) (time >> 8);
		output[6] = (u_char) (time);
		result = send_adb_cuda((u_char *) output,
		    (u_char *) 0, (void *) adb_op_comprout,
		    (void *) &flag, (int) 0);
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
	u_char output[MAX_ADB_MSG_LENGTH];
	int result;

	switch (adbHardware) {
	case ADB_HW_IISI:
		output[0] = 0x02;	/* 2 byte message */
		output[1] = 0x01;	/* to pram/rtc/soft-power device */
		output[2] = 0x0a;	/* set date/time */
		result = send_adb_IIsi((u_char *) output,
		    (u_char *) 0, (void *) 0, (void *) 0, (int) 0);
		if (result != 0)	/* exit if not sent */
			return -1;

		for (;;);		/* wait for power off */

		return 0;

	case ADB_HW_PB:
		return -1;

	/* TO DO: some cuda models claim to do soft power - check out */
	case ADB_HW_II:			/* II models don't do soft power */
	case ADB_HW_CUDA:		/* cuda doesn't do soft power */
	case ADB_HW_UNKNOWN:
	default:
		return -1;
	}
}

int 
adb_prog_switch_enable(void)
{
	u_char output[MAX_ADB_MSG_LENGTH];
	int result;
	volatile int flag = 0;

	switch (adbHardware) {
	case ADB_HW_IISI:
		output[0] = 0x03;	/* 3 byte message */
		output[1] = 0x01;	/* to pram/rtc/soft-power device */
		output[2] = 0x1c;	/* prog. switch control */
		output[3] = 0x01;	/* enable */
		result = send_adb_IIsi((u_char *) output,
		    (u_char *) 0, (void *) adb_op_comprout,
		    (void *) &flag, (int) 0);
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
	u_char output[MAX_ADB_MSG_LENGTH];
	int result;
	volatile int flag = 0;

	switch (adbHardware) {
	case ADB_HW_IISI:
		output[0] = 0x03;	/* 3 byte message */
		output[1] = 0x01;	/* to pram/rtc/soft-power device */
		output[2] = 0x1c;	/* prog. switch control */
		output[3] = 0x01;	/* disable */
		result = send_adb_IIsi((u_char *) output,
		    (u_char *) 0, (void *) adb_op_comprout,
		    (void *) &flag, (int) 0);
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

#ifndef MRG_ADB

int 
CountADBs(void)
{
	return (count_adbs());
}

void 
ADBReInit(void)
{
	adb_reinit();
}

int 
GetIndADB(ADBDataBlock * info, int index)
{
	return (get_ind_adb_info(info, index));
}

int 
GetADBInfo(ADBDataBlock * info, int adbAddr)
{
	return (get_adb_info(info, adbAddr));
}

int 
SetADBInfo(ADBSetInfoBlock * info, int adbAddr)
{
	return (set_adb_info(info, adbAddr));
}

int 
ADBOp(Ptr buffer, Ptr compRout, Ptr data, short commandNum)
{
	return (adb_op(buffer, compRout, data, commandNum));
}

#endif

