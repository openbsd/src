/* 
 * $OpenBSD: drcustom.h,v 1.3 1996/08/19 00:04:13 niklas Exp $
 * $NetBSD: drcustom.h,v 1.1.4.1 1996/06/21 06:42:44 jtc Exp $
 * 
 * Motherboard addresses for the DraCo.
 *
 */

#ifndef _DRACO_HARDWARE
#define _DRACO_HARDWARE
 
/*
 * CIA-B is available only in very early models.
 * CIA-A is available only up to revision 3.
 */

#define DRCIABASE 0x02800000
#define DRCIATOP  0x02802000
#define NDRCIAPG ((DRCIATOP - DRCIABASE) / NBPG)	/* which is 1 */

#define NDRCCPG (8+1+1) /* (3 int+msc+ctrl+superio+cia+1stkick)+scsi+altais */

#define DRCCBASE	0x01000000
#define DRCCSTRIDE	0x00400000	/* for up to and including 1st kick pg */

#define DRZ2BASE	0x03000000	/*
					 * not really used, appears as Z3 to
					 * our kernel.
					 */
#define DRSCSIBASE	0x04000000

#define DR_INTENA (DRCCBASE+0x1)
#define DR_INTPEN (DRCCBASE+0x00400001)
#define DR_INTFRC (DRCCBASE+0x00800001)

#define DRIRQ_GLOBAL	1	/* not force */
#define DRIRQ_SOFT DRIRQ_GLOBAL	/* only force */
#define DRIRQ_SCSI	2
#define DRIRQ_INT2	4
#define DRIRQ_INT6	8


/* mapped state: */
#define	DRMISCPG 3
#define	DRIOCTLPG 4
#define	DRSUPIOPG 5
#define DRCIAPG 6
#define DRKICKPG 7	/*
			 * kick page, used only as a stopgap delay address
			 * for early DraCos
			 */
#define DRSCSIPG 8

#ifdef _KERNEL
#ifndef _LOCORE

vm_offset_t DRCCADDR;

volatile u_int8_t *draco_intena, *draco_intpen, *draco_intfrc;
volatile u_int8_t *draco_misc;
volatile struct drioct *draco_ioct;

struct drioct {
	u_int8_t dum0;
	volatile u_int8_t io_control;	/*  1 */
#define DRCNTRL_FDCINTENA 1
#define DRCNTRL_KBDDATOUT 2
#define DRCNTRL_KBDCLKOUT 4
#define DRCNTRL_WDOGENA 8
#define DRCNTRL_WDOGDAT 16
#define DRCNTRL_KBDINTENA 32
#define DRCNTRL_KBDKBDACK 64
#define DRCNTRL_SCSITERM 128

	u_int8_t dum1;
	volatile u_int8_t io_status;	/*  3 */
#define DRSTAT_CLKDAT 1
#define DRSTAT_KBDDATIN 2
#define DRSTAT_KBDCLKIN 4
#define DRSTAT_KBDRECV 8
#define DRSTAT_CLKBUSY 16
#define DRSTAT_BUSTIMO 32
#define DRSTAT_SCSILED 64

	u_int8_t dum2;
	volatile u_int8_t io_kbddata;	/*  5 */

	u_int8_t dum3;
	volatile u_int8_t io_status2;	/*  7 */
#define DRSTAT2_KBDBUSY 1
#define DRSTAT2_PARIRQPEN 4
#define DRSTAT2_PARIRQENA 8
#define DRSTAT2_TMRINTENA 16
#define DRSTAT2_TMRIRQPEN 32

	u_int8_t dum4;
	volatile u_int8_t io_chiprev;	/*  9 */
#define io_timerrst io_chiprev		/* on writes */

	u_int8_t dum5;
	volatile u_int8_t io_timerhi;	/*  b */
	u_int8_t dum6;
	volatile u_int8_t io_timerlo;	/*  d */

	u_int8_t dum7[3];	/* nothing @ f, at least yet */

	volatile u_int8_t io_clockw0;	/* 11 */
	u_int8_t dum8;
	volatile u_int8_t io_clockw1;	/* 13 */
	u_int8_t dum9;
	volatile u_int8_t io_clockrst;	/* 15 */

	u_int8_t dum10;
	volatile u_int8_t io_kbdrst;	/* 17 */

	u_int8_t dum11;
	volatile u_int8_t io_bustimeoutrst;	/* 19 */

	u_int8_t dum12;
	volatile u_int8_t io_scsiledrst;	/* 1b */

	u_int8_t dum13;
	volatile u_int8_t io_fdcread;		/* 1d */

	u_int8_t dum14;
	volatile u_int8_t io_parrst;		/* 1e */

};
#endif
#endif

#endif
