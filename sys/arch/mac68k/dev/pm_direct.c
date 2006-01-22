/*	$OpenBSD: pm_direct.c,v 1.12 2006/01/22 15:25:30 miod Exp $	*/
/*	$NetBSD: pm_direct.c,v 1.25 2005/10/28 21:54:52 christos Exp $	*/

/*
 * Copyright (C) 1997 Takashi Hamada
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
 *  This product includes software developed by Takashi Hamada
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
/* From: pm_direct.c 1.3 03/18/98 Takashi Hamada */

#ifdef DEBUG
#ifndef ADB_DEBUG
#define ADB_DEBUG
#endif
#endif

/* #define	PM_GRAB_SI	1 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/viareg.h>

#include <dev/adb/adb.h>
#include <mac68k/dev/adbvar.h>
#include <mac68k/dev/pm_direct.h>

/* hardware dependent values */
u_int32_t HwCfgFlags3;
u_short ADBDelay = 0xcea;

/* define the types of the Power Manager */
#define PM_HW_UNKNOWN		0x00	/* don't know */
#define PM_HW_PB1XX		0x01	/* PowerBook 1XX series */
#define	PM_HW_PB5XX		0x02	/* PowerBook Duo and 5XX series */

/* useful macros */
#define PM_SR()			via_reg(VIA1, vSR)
#define PM_VIA_INTR_ENABLE()	via_reg(VIA1, vIER) = 0x90
#define PM_VIA_INTR_DISABLE()	via_reg(VIA1, vIER) = 0x10
#define PM_VIA_CLR_INTR()	via_reg(VIA1, vIFR) = 0x90
#define PM_SET_STATE_ACKON()	via_reg(VIA2, vBufB) |= 0x04
#define PM_SET_STATE_ACKOFF()	via_reg(VIA2, vBufB) &= ~0x04
#define PM_IS_ON		(0x02 == (via_reg(VIA2, vBufB) & 0x02))
#define PM_IS_OFF		(0x00 == (via_reg(VIA2, vBufB) & 0x02))

/*
 * Variables for internal use
 */
int	pmHardware = PM_HW_UNKNOWN;
u_short	pm_existent_ADB_devices = 0x0;	/* each bit expresses the existent ADB device */
u_int	pm_LCD_brightness = 0x0;
u_int	pm_LCD_contrast = 0x0;
u_int	pm_counter = 0;			/* clock count */

/* these values shows that number of data returned after 'send' cmd is sent */
char pm_send_cmd_type[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x01, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
	0xff, 0x00, 0x02, 0x01, 0x01, 0xff, 0xff, 0xff,
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x04, 0x14, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x02, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x01, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x01, 0x00, 0x02, 0x02, 0xff, 0x01, 0x03, 0x01,
	0x00, 0x01, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff,
	0x02, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
	0x01, 0x01, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x04, 0x04,
	0x04, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x01, 0x02, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x02, 0x02, 0x02, 0x04, 0xff, 0x00, 0xff, 0xff,
	0x01, 0x01, 0x03, 0x02, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x01, 0x01, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff,
	0xff, 0x04, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x03, 0xff, 0x00, 0xff, 0x00, 0xff, 0xff, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

/* these values shows that number of data returned after 'receive' cmd is sent */
char pm_receive_cmd_type[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x02, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x05, 0x15, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x02, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x00, 0x03, 0x03, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x04, 0x04, 0x03, 0x09, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x06, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x02, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x02, 0xff, 0xff, 0x02, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0x02, 0xff, 0xff, 0xff, 0xff, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};


/*
 * Define the private functions
 */

/* for debugging */
#ifdef ADB_DEBUG
void	pm_printerr(const char *, int, int, char *);
#endif

int	pm_wait_busy(int);
int	pm_wait_free(int);

/* these functions are for the PB1XX series */
int	pm_receive_pm1(u_char *);
int	pm_send_pm1(u_char, int);
int	pm_pmgrop_pm1(PMData *);
int	pm_intr_pm1(void *);

/* these functions are for the PB Duo series and the PB 5XX series */
int	pm_receive_pm2(u_char *);
int	pm_send_pm2(u_char);
int	pm_pmgrop_pm2(PMData *);
int	pm_intr_pm2(void *);

/* these functions are called from adb_direct.c */
void	pm_setup_adb(void);
void	pm_check_adb_devices(int);
int	pm_intr(void *);
int	pm_adb_op(u_char *, void *, void *, int);
void	pm_hw_setup(struct device *);

/* these functions also use the variables of adb_direct.c */
void	pm_adb_get_TALK_result(PMData *);
void	pm_adb_get_ADB_data(PMData *);
void	pm_adb_poll_next_device_pm1(PMData *);


/*
 * These variables are in adb_direct.c.
 */
extern u_char	*adbBuffer;	/* pointer to user data area */
extern void	*adbCompRout;	/* pointer to the completion routine */
extern void	*adbCompData;	/* pointer to the completion routine data */
extern int	adbWaiting;	/* waiting for return data from the device */
extern int	adbWaitingCmd;	/* ADB command we are waiting for */
extern int	adbStarting;	/* doing ADB reinit, so do "polling" differently */

#define	ADB_MAX_MSG_LENGTH	16
#define	ADB_MAX_HDR_LENGTH	8
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
extern	void	adb_pass_up(struct adbCommand *);

#ifdef ADB_DEBUG
/*
 * This function dumps contents of the PMData
 */
void
pm_printerr(const char *ttl, int rval, int num, char *data)
{
	int i;

	printf("pm: %s:%04x %02x ", ttl, rval, num);
	for (i = 0; i < num; i++)
		printf("%02x ", data[i]);
	printf("\n");
}
#endif



/*
 * Check the hardware type of the Power Manager
 */
void
pm_setup_adb(void)
{
	switch (mac68k_machine.machineid) {
		case MACH_MACPB140:
		case MACH_MACPB145:
		case MACH_MACPB160:
		case MACH_MACPB165:
		case MACH_MACPB165C:
		case MACH_MACPB170:
		case MACH_MACPB180:
		case MACH_MACPB180C:
			pmHardware = PM_HW_PB1XX;
			break;
		case MACH_MACPB150:
		case MACH_MACPB210:
		case MACH_MACPB230:
		case MACH_MACPB250:
		case MACH_MACPB270:
		case MACH_MACPB280:
		case MACH_MACPB280C:
		case MACH_MACPB500:
		case MACH_MACPB190:
		case MACH_MACPB190CS:
			pmHardware = PM_HW_PB5XX;
			break;
		default:
			break;
	}
}


/*
 * Check the existent ADB devices
 */
void
pm_check_adb_devices(int id)
{
	u_short ed = 0x1;

	ed <<= id;
	pm_existent_ADB_devices |= ed;
}


/*
 * Wait until PM IC is busy
 */
int
pm_wait_busy(int xdelay)
{
	while (PM_IS_ON) {
#ifdef PM_GRAB_SI
		(void)intr_dispatch(0x70);	/* grab any serial interrupts */
#endif
		if ((--xdelay) < 0)
			return 1;	/* timeout */
	}
	return 0;
}


/*
 * Wait until PM IC is free
 */
int
pm_wait_free(int xdelay)
{
	while (PM_IS_OFF) {
#ifdef PM_GRAB_SI
		(void)intr_dispatch(0x70);	/* grab any serial interrupts */
#endif
		if ((--xdelay) < 0)
			return 0;	/* timeout */
	}
	return 1;
}



/*
 * Functions for the PB1XX series
 */

/*
 * Receive data from PM for the PB1XX series
 */
int
pm_receive_pm1(u_char *data)
{
	int rval = 0xffffcd34;

	via_reg(VIA2, vDirA) = 0x00;

	switch (1) {
		default:
			if (pm_wait_busy(0x40) != 0)
				break;			/* timeout */

			PM_SET_STATE_ACKOFF();
			*data = via_reg(VIA2, 0x200);

			rval = 0xffffcd33;
			if (pm_wait_free(0x40) == 0)
				break;			/* timeout */

			rval = 0x00;
			break;
	}

	PM_SET_STATE_ACKON();
	via_reg(VIA2, vDirA) = 0x00;

	return rval;
}



/*
 * Send data to PM for the PB1XX series
 */
int
pm_send_pm1(u_char data, int timo)
{
	int rval;

	via_reg(VIA2, vDirA) = 0xff;
	via_reg(VIA2, 0x200) = data;

	PM_SET_STATE_ACKOFF();
#if 0
	if (pm_wait_busy(0x400) == 0) {
#else
	if (pm_wait_busy(timo) == 0) {
#endif
		PM_SET_STATE_ACKON();
		if (pm_wait_free(0x40) != 0)
			rval = 0x0;
		else
			rval = 0xffffcd35;
	} else {
		rval = 0xffffcd36;
	}

	PM_SET_STATE_ACKON();
	via_reg(VIA2, vDirA) = 0x00;

	return rval;
}


/*
 * My PMgrOp routine for the PB1XX series
 */
int
pm_pmgrop_pm1(PMData *pmdata)
{
	int i;
	int s = 0x81815963;
	u_char via1_vIER, via1_vDirA;
	int rval = 0;
	int num_pm_data = 0;
	u_char pm_cmd;	
	u_char pm_data;
	u_char *pm_buf;

	/* disable all inetrrupts but PM */
	via1_vIER = via_reg(VIA1, vIER);
	PM_VIA_INTR_DISABLE();

	via1_vDirA = via_reg(VIA1, vDirA);

	switch (pmdata->command) {
		default:
			for (i = 0; i < 7; i++) {
				via_reg(VIA2, vDirA) = 0x00;	

				/* wait until PM is free */
				if (pm_wait_free(ADBDelay) == 0) {	/* timeout */
					via_reg(VIA2, vDirA) = 0x00;
					/* restore formar value */
					via_reg(VIA1, vDirA) = via1_vDirA;
					via_reg(VIA1, vIER) = via1_vIER;
					return 0xffffcd38;
				}

				switch (mac68k_machine.machineid) {
					case MACH_MACPB160:
					case MACH_MACPB165:
					case MACH_MACPB165C:
					case MACH_MACPB170:
					case MACH_MACPB180:
					case MACH_MACPB180C:
						{
							int xdelay = ADBDelay * 16;

							via_reg(VIA2, vDirA) = 0x00;
							while ((via_reg(VIA2, 0x200) == 0x7f) && (xdelay >= 0))
								xdelay--;

							if (xdelay < 0) {	/* timeout */
								via_reg(VIA2, vDirA) = 0x00;
								/* restore formar value */
								via_reg(VIA1, vIER) = via1_vIER;
								return 0xffffcd38;
							}
						}
				} /* end switch */

				s = splhigh();

				via1_vDirA = via_reg(VIA1, vDirA);
				via_reg(VIA1, vDirA) &= 0x7f;

				pm_cmd = (u_char)(pmdata->command & 0xff);
				if ((rval = pm_send_pm1(pm_cmd, ADBDelay * 8)) == 0)
					break;	/* send command succeeded */

				via_reg(VIA1, vDirA) = via1_vDirA;
				splx(s);
			} /* end for */

			/* failed to send a command */
			if (i == 7) {
				via_reg(VIA2, vDirA) = 0x00;
				/* restore formar value */
				via_reg(VIA1, vDirA) = via1_vDirA;
				via_reg(VIA1, vIER) = via1_vIER;
				if (s != 0x81815963)
					splx(s);
				return 0xffffcd38;
			}

			/* send # of PM data */
			num_pm_data = pmdata->num_data;
			if ((rval = pm_send_pm1((u_char)(num_pm_data & 0xff), ADBDelay * 8)) != 0)
				break;			/* timeout */

			/* send PM data */
			pm_buf = (u_char *)pmdata->s_buf;
			for (i = 0; i < num_pm_data; i++)
				if ((rval = pm_send_pm1(pm_buf[i], ADBDelay * 8)) != 0)
					break;		/* timeout */
			if ((i != num_pm_data) && (num_pm_data != 0))
				break;			/* timeout */

			/* Will PM IC return data? */
			if ((pm_cmd & 0x08) == 0) {
				rval = 0;
				break;			/* no returned data */
			}

			rval = 0xffffcd37;
			if (pm_wait_busy(ADBDelay) != 0)
				break;			/* timeout */

			/* receive PM command */
			if ((rval = pm_receive_pm1(&pm_data)) != 0)
				break;

			pmdata->command = pm_data;

			/* receive number of PM data */
			if ((rval = pm_receive_pm1(&pm_data)) != 0)
				break;			/* timeout */
			num_pm_data = pm_data;
			pmdata->num_data = num_pm_data;

			/* receive PM data */
			pm_buf = (u_char *)pmdata->r_buf;
			for (i = 0; i < num_pm_data; i++) {
				if ((rval = pm_receive_pm1(&pm_data)) != 0)
					break;		/* timeout */
				pm_buf[i] = pm_data;
			}

			rval = 0;
	}

	via_reg(VIA2, vDirA) = 0x00;	

	/* restore formar value */
	via_reg(VIA1, vDirA) = via1_vDirA;
	via_reg(VIA1, vIER) = via1_vIER;
	if (s != 0x81815963)
		splx(s);

	return rval;
}


/*
 * My PM interrupt routine for PB1XX series
 */
int
pm_intr_pm1(void *arg)
{
	int s;
	int rval;
	PMData pmdata;

	s = splhigh();

	PM_VIA_CLR_INTR();				/* clear VIA1 interrupt */

	/* ask PM what happend */
	pmdata.command = 0x78;
	pmdata.num_data = 0;
	pmdata.data[0] = pmdata.data[1] = 0;
	pmdata.s_buf = &pmdata.data[2];
	pmdata.r_buf = &pmdata.data[2];
	rval = pm_pmgrop_pm1(&pmdata);
	if (rval != 0) {
#ifdef ADB_DEBUG
		if (adb_debug)
			printf("pm: PM is not ready. error code=%08x\n", rval);
#endif
		splx(s);
	}

	if ((pmdata.data[2] & 0x10) == 0x10) {
		if ((pmdata.data[2] & 0x0f) == 0) {
			/* ADB data that were requested by TALK command */
			pm_adb_get_TALK_result(&pmdata);
		} else if ((pmdata.data[2] & 0x08) == 0x8) {
			/* PM is requesting to poll  */
			pm_adb_poll_next_device_pm1(&pmdata);
		} else if ((pmdata.data[2] & 0x04) == 0x4) {
			/* ADB device event */
			pm_adb_get_ADB_data(&pmdata);
		}
	} else {
#ifdef ADB_DEBUG
		if (adb_debug)
			pm_printerr("driver does not supported this event.",
			    rval, pmdata.num_data, pmdata.data);
#endif
	}

	splx(s);

	return (1);
}



/*
 * Functions for the PB Duo series and the PB 5XX series
 */

/*
 * Receive data from PM for the PB Duo series and the PB 5XX series
 */
int
pm_receive_pm2(u_char *data)
{
	int i;
	int rval;

	rval = 0xffffcd34;

	switch (1) {
		default:
			/* set VIA SR to input mode */
			via_reg(VIA1, vACR) |= 0x0c;
			via_reg(VIA1, vACR) &= ~0x10;
			i = PM_SR();

			PM_SET_STATE_ACKOFF();
			if (pm_wait_busy((int)ADBDelay*32) != 0)
				break;		/* timeout */

			PM_SET_STATE_ACKON();
			rval = 0xffffcd33;
			if (pm_wait_free((int)ADBDelay*32) == 0)
				break;		/* timeout */

			*data = PM_SR();
			rval = 0;

			break;
	}

	PM_SET_STATE_ACKON();
	via_reg(VIA1, vACR) |= 0x1c;

	return rval;
}	



/*
 * Send data to PM for the PB Duo series and the PB 5XX series
 */
int
pm_send_pm2(u_char data)
{
	int rval;

	via_reg(VIA1, vACR) |= 0x1c;
	PM_SR() = data;

	PM_SET_STATE_ACKOFF();
	if (pm_wait_busy((int)ADBDelay*32) == 0) {
		PM_SET_STATE_ACKON();
		if (pm_wait_free((int)ADBDelay*32) != 0)
			rval = 0;
		else
			rval = 0xffffcd35;
	} else {
		rval = 0xffffcd36;
	}

	PM_SET_STATE_ACKON();
	via_reg(VIA1, vACR) |= 0x1c;

	return rval;
}



/*
 * My PMgrOp routine for the PB Duo series and the PB 5XX series
 */
int
pm_pmgrop_pm2(PMData *pmdata)
{
	int i;
	int s;
	u_char via1_vIER;
	int rval = 0;
	int num_pm_data = 0;
	u_char pm_cmd;	
	short pm_num_rx_data;
	u_char pm_data;
	u_char *pm_buf;

	s = splhigh();

	/* disable all inetrrupts but PM */
	via1_vIER = 0x10;
	via1_vIER &= via_reg(VIA1, vIER);
	via_reg(VIA1, vIER) = via1_vIER;
	if (via1_vIER != 0x0)
		via1_vIER |= 0x80;

	switch (pmdata->command) {
		default:
			/* wait until PM is free */
			pm_cmd = (u_char)(pmdata->command & 0xff);
			rval = 0xcd38;
			if (pm_wait_free(ADBDelay * 4) == 0)
				break;			/* timeout */

			if (HwCfgFlags3 & 0x00200000) {	
				/* PB 160, PB 165(c), PB 180(c)? */
				int xdelay = ADBDelay * 16;

				via_reg(VIA2, vDirA) = 0x00;
				while ((via_reg(VIA2, 0x200) == 0x07) &&
				    (xdelay >= 0))
					xdelay--;

				if (xdelay < 0) {
					rval = 0xffffcd38;
					break;		/* timeout */
				}
			}

			/* send PM command */
			if ((rval = pm_send_pm2((u_char)(pm_cmd & 0xff))))
				break;				/* timeout */

			/* send number of PM data */
			num_pm_data = pmdata->num_data;
			if (HwCfgFlags3 & 0x00020000) {		/* PB Duo, PB 5XX */
				if (pm_send_cmd_type[pm_cmd] < 0) {
					if ((rval = pm_send_pm2((u_char)(num_pm_data & 0xff))) != 0)
						break;		/* timeout */
					pmdata->command = 0;
				}
			} else {				/* PB 1XX series ? */
				if ((rval = pm_send_pm2((u_char)(num_pm_data & 0xff))) != 0)
					break;			/* timeout */
			}			
			/* send PM data */
			pm_buf = (u_char *)pmdata->s_buf;
			for (i = 0 ; i < num_pm_data; i++)
				if ((rval = pm_send_pm2(pm_buf[i])) != 0)
					break;			/* timeout */
			if (i != num_pm_data)
				break;				/* timeout */


			/* check if PM will send me data  */
			pm_num_rx_data = pm_receive_cmd_type[pm_cmd];
			pmdata->num_data = pm_num_rx_data;
			if (pm_num_rx_data == 0) {
				rval = 0;
				break;				/* no return data */
			}

			/* receive PM command */
			pm_data = pmdata->command;
			if (HwCfgFlags3 & 0x00020000) {		/* PB Duo, PB 5XX */
				pm_num_rx_data--;
				if (pm_num_rx_data == 0)
					if ((rval = pm_receive_pm2(&pm_data)) != 0) {
						rval = 0xffffcd37;
						break;
					}
				pmdata->command = pm_data;
			} else {				/* PB 1XX series ? */
				if ((rval = pm_receive_pm2(&pm_data)) != 0) {
					rval = 0xffffcd37;
					break;
				}
				pmdata->command = pm_data;
			}

			/* receive number of PM data */
			if (HwCfgFlags3 & 0x00020000) {		/* PB Duo, PB 5XX */
				if (pm_num_rx_data < 0) {
					if ((rval = pm_receive_pm2(&pm_data)) != 0)
						break;		/* timeout */
					num_pm_data = pm_data;
				} else
					num_pm_data = pm_num_rx_data;
				pmdata->num_data = num_pm_data;
			} else {				/* PB 1XX serias ? */
				if ((rval = pm_receive_pm2(&pm_data)) != 0)
					break;			/* timeout */
				num_pm_data = pm_data;
				pmdata->num_data = num_pm_data;
			}

			/* receive PM data */
			pm_buf = (u_char *)pmdata->r_buf;
			for (i = 0; i < num_pm_data; i++) {
				if ((rval = pm_receive_pm2(&pm_data)) != 0)
					break;			/* timeout */
				pm_buf[i] = pm_data;
			}

			rval = 0;
	}

	/* restore former value */
	via_reg(VIA1, vIER) = via1_vIER;
	splx(s);

	return rval;
}


/*
 * My PM interrupt routine for the PB Duo series and the PB 5XX series
 */
int
pm_intr_pm2(void *arg)
{
	int s;
	int rval;
	PMData pmdata;

	s = splhigh();

	PM_VIA_CLR_INTR();			/* clear VIA1 interrupt */
						/* ask PM what happend */
	pmdata.command = 0x78;
	pmdata.num_data = 0;
	pmdata.s_buf = &pmdata.data[2];
	pmdata.r_buf = &pmdata.data[2];
	rval = pm_pmgrop_pm2(&pmdata);
	if (rval != 0) {
#ifdef ADB_DEBUG
		if (adb_debug)
			printf("pm: PM is not ready. error code: %08x\n", rval);
#endif
		splx(s);
	}

	switch ((u_int)(pmdata.data[2] & 0xff)) {
		case 0x00:			/* 1 sec interrupt? */
			break;
		case 0x80:			/* 1 sec interrupt? */
			pm_counter++;
			break;
		case 0x08:			/* Brightness/Contrast button on LCD panel */
			/* get brightness and contrast of the LCD */
			pm_LCD_brightness = (u_int)pmdata.data[3] & 0xff;
			pm_LCD_contrast = (u_int)pmdata.data[4] & 0xff;
/*
			pm_printerr("#08", rval, pmdata.num_data, pmdata.data);
			pmdata.command = 0x33;
			pmdata.num_data = 1;
			pmdata.s_buf = pmdata.data;
			pmdata.r_buf = pmdata.data;
			pmdata.data[0] = pm_LCD_contrast;
			rval = pm_pmgrop_pm2(&pmdata);
			pm_printerr("#33", rval, pmdata.num_data, pmdata.data);
*/
			/* this is an experimental code */
			pmdata.command = 0x41;
			pmdata.num_data = 1;
			pmdata.s_buf = pmdata.data;
			pmdata.r_buf = pmdata.data;
			pm_LCD_brightness = 0x7f - pm_LCD_brightness / 2;
			if (pm_LCD_brightness < 0x25)
				pm_LCD_brightness = 0x25;
			if (pm_LCD_brightness > 0x5a)
				pm_LCD_brightness = 0x7f;
			pmdata.data[0] = pm_LCD_brightness;
			rval = pm_pmgrop_pm2(&pmdata);
			break;
		case 0x10:			/* ADB data that were requested by TALK command */
		case 0x14:
			pm_adb_get_TALK_result(&pmdata);
			break;
		case 0x16:			/* ADB device event */
		case 0x18:
		case 0x1e:
			pm_adb_get_ADB_data(&pmdata);
			break;
		default:
#ifdef ADB_DEBUG
			if (adb_debug)
				pm_printerr("driver does not supported this event.",
				    pmdata.data[2], pmdata.num_data,
				    pmdata.data);
#endif
			break;
	}

	splx(s);

	return (1);
}


/*
 * My PMgrOp routine
 */
int
pmgrop(PMData *pmdata)
{
	switch (pmHardware) {
		case PM_HW_PB1XX:
			return (pm_pmgrop_pm1(pmdata));
			break;
		case PM_HW_PB5XX:
			return (pm_pmgrop_pm2(pmdata));
			break;
		default:
			return 1;
	}
}

int
pm_intr(void *arg)
{
	switch (pmHardware) {
	case PM_HW_PB1XX:
		return (pm_intr_pm1(arg));
	case PM_HW_PB5XX:
		return (pm_intr_pm2(arg));
	default:
		return (-1);
	}
}

void
pm_hw_setup(struct device *self)
{
	switch (pmHardware) {
	case PM_HW_PB1XX:
		via1_register_irq(4, pm_intr_pm1, self, self->dv_xname);
		PM_VIA_CLR_INTR();
		break;
	case PM_HW_PB5XX:
		via1_register_irq(4, pm_intr_pm2, self, self->dv_xname);
		PM_VIA_CLR_INTR();
		break;
	default:
		break;
	}
}


/*
 * Synchronous ADBOp routine for the Power Manager
 */
int
pm_adb_op(u_char *buffer, void *compRout, void *data, int command)
{
	int i;
	int s;
	int rval;
	int xdelay;
	PMData pmdata;
	struct adbCommand packet;

	if (adbWaiting == 1)
		return 1;

	s = splhigh();
	via_reg(VIA1, vIER) = 0x10;

 	adbBuffer = buffer;
	adbCompRout = compRout;
	adbCompData = data;

	pmdata.command = 0x20;
	pmdata.s_buf = pmdata.data;
	pmdata.r_buf = pmdata.data;

	if ((command & 0xc) == 0x8) {		/* if the command is LISTEN, add number of ADB data to number of PM data */
		if (buffer != (u_char *)0)
			pmdata.num_data = buffer[0] + 3;
	} else {
		pmdata.num_data = 3;
	}

	pmdata.data[0] = (u_char)(command & 0xff);
	pmdata.data[1] = 0;
	if ((command & 0xc) == 0x8) {		/* if the command is LISTEN, copy ADB data to PM buffer */
		if ((buffer != (u_char *)0) && (buffer[0] <= 24)) {
			pmdata.data[2] = buffer[0];		/* number of data */
			for (i = 0; i < buffer[0]; i++)
				pmdata.data[3 + i] = buffer[1 + i];
		} else
			pmdata.data[2] = 0;
	} else
		pmdata.data[2] = 0;

	if ((command & 0xc) != 0xc) {		/* if the command is not TALK */
		/* set up stuff fNULLor adb_pass_up */
		packet.data[0] = 1 + pmdata.data[2];
		packet.data[1] = command;
		for (i = 0; i < pmdata.data[2]; i++)
			packet.data[i+2] = pmdata.data[i+3];
		packet.saveBuf = adbBuffer;
		packet.compRout = adbCompRout;
		packet.compData = adbCompData;
		packet.cmd = command;
		packet.unsol = 0;
		packet.ack_only = 1;
		adb_polling = 1;
		adb_pass_up(&packet);
		adb_polling = 0;
	}

	rval = pmgrop(&pmdata);
	if (rval != 0) {
		splx(s);
		return 1;
	}

	adbWaiting = 1;
	adbWaitingCmd = command;

	PM_VIA_INTR_ENABLE();

	/* wait until the PM interrupt has occurred */
	xdelay = 0x80000;
	while (adbWaiting == 1) {
		switch (mac68k_machine.machineid) {
		case MACH_MACPB150:
		case MACH_MACPB210:
		case MACH_MACPB230:	/* daishi tested with Duo230 */
		case MACH_MACPB250:
		case MACH_MACPB270:
		case MACH_MACPB280:
		case MACH_MACPB280C:
		case MACH_MACPB190:
		case MACH_MACPB190CS:
			pm_intr((void *)0);
			break;
		default:
			if ((via_reg(VIA1, vIFR) & 0x10) == 0x10)
				pm_intr((void *)0);
			break;
		}
#ifdef PM_GRAB_SI
		(void)intr_dispatch(0x70);	/* grab any serial interrupts */
#endif
		if ((--xdelay) < 0) {
			splx(s);
			return 1;
		}
	}

	/* this command enables the interrupt by operating ADB devices */
	if (HwCfgFlags3 & 0x00020000) {		/* PB Duo series, PB 5XX series */
		pmdata.command = 0x20;
		pmdata.num_data = 4;
		pmdata.s_buf = pmdata.data;
		pmdata.r_buf = pmdata.data;
		pmdata.data[0] = 0x00;	
		pmdata.data[1] = 0x86;	/* magic spell for awaking the PM */
		pmdata.data[2] = 0x00;	
		pmdata.data[3] = 0x0c;	/* each bit may express the existent ADB device */
	} else {				/* PB 1XX series */
		pmdata.command = 0x20;
		pmdata.num_data = 3;
		pmdata.s_buf = pmdata.data;
		pmdata.r_buf = pmdata.data;
		pmdata.data[0] = (u_char)(command & 0xf0) | 0xc;
		pmdata.data[1] = 0x04;
		pmdata.data[2] = 0x00;
	}
	rval = pmgrop(&pmdata);

	splx(s);
	return rval;
}


void
pm_adb_get_TALK_result(PMData *pmdata)
{
	int i;
	struct adbCommand packet;

	/* set up data for adb_pass_up */
	packet.data[0] = pmdata->num_data-1;
	packet.data[1] = pmdata->data[3];
	for (i = 0; i <packet.data[0]-1; i++)
		packet.data[i+2] = pmdata->data[i+4];

	packet.saveBuf = adbBuffer;
	packet.compRout = adbCompRout;
	packet.compData = adbCompData;
	packet.unsol = 0;
	packet.ack_only = 0;
	adb_polling = 1;
	adb_pass_up(&packet);
	adb_polling = 0;

	adbWaiting = 0;
	adbBuffer = (long)0;
	adbCompRout = (long)0;
	adbCompData = (long)0;
}


void
pm_adb_get_ADB_data(PMData *pmdata)
{
	int i;
	struct adbCommand packet;

	/* set up data for adb_pass_up */
	packet.data[0] = pmdata->num_data-1;	/* number of raw data */
	packet.data[1] = pmdata->data[3];	/* ADB command */
	for (i = 0; i <packet.data[0]-1; i++)
		packet.data[i+2] = pmdata->data[i+4];
	packet.unsol = 1;
	packet.ack_only = 0;
	adb_pass_up(&packet);
}


void
pm_adb_poll_next_device_pm1(PMData *pmdata)
{
	int i;
	int ndid;
	u_short bendid = 0x1;
	int rval;
	PMData tmp_pmdata;

	/* find another existent ADB device to poll */
	for (i = 1; i < 16; i++) {
		ndid = (ADB_CMDADDR(pmdata->data[3]) + i) & 0xf;
		bendid <<= ndid;
		if ((pm_existent_ADB_devices & bendid) != 0)
			break;
	}

	/* poll the other device */
	tmp_pmdata.command = 0x20;
	tmp_pmdata.num_data = 3;
	tmp_pmdata.s_buf = tmp_pmdata.data;
	tmp_pmdata.r_buf = tmp_pmdata.data;
	tmp_pmdata.data[0] = (u_char)(ndid << 4) | 0xc;
	tmp_pmdata.data[1] = 0x04;	/* magic spell for awaking the PM */
	tmp_pmdata.data[2] = 0x00;
	rval = pmgrop(&tmp_pmdata);
}
