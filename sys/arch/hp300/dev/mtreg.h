/*	$OpenBSD: mtreg.h,v 1.4 2005/01/15 21:13:08 miod Exp $	*/
/*	$NetBSD: mtreg.h,v 1.1 1995/10/02 00:28:22 thorpej Exp $	*/

/*
 * Copyright (c) 1992, The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *	Utah $Hdr: mtreg.h 1.4 95/09/12$
 */
/*	@(#)mtreg.h	3.4	90/07/10	mt Xinu
 *
 *	Hewlett-Packard 7974, 7978, 7979 and 7980 HPIB Mag-Tape declarations.
 */

/*
 *	Hardware Id's
 */

#define MT7974AID	0x174
#define MT7978ID	0x178
#define MT7979AID	0x179
#define MT7980ID	0x180

/* convert bytes to 1k tape block and back */
#define CTBTOK(x)	((x) >> 10)
#define CTKTOB(x)	((x) << 10)

/*
 *	Listen Secondary Commands
 */

#define MTL_WRITE	0	/* write execute */
#define MTL_TCMD	1	/* tape command */
#define MTL_DIAG	4	/* download diagnostic */
#define MTL_FUP		6	/* write firmware update */
#define MTL_ECMD	7	/* end command */
#define MTL_DCL		16	/* amigo device clear */
#define MTL_CCRC	17	/* clear CRC */
#define MTL_XTEST	29	/* run 7979a/7980 extended self test */
#define MTL_LOOP	30	/* write interface loopback */
#define MTL_TEST	31	/* run self test */

/*
 *	Talk Secondary Commands
 */

#define MTT_READ	0	/* read execute */
#define MTT_STAT	1	/* read status */
#define MTT_BCNT	2	/* read byte count */
#define MTT_DIAG	3	/* read diagnostic results */
#define MTT_FREV	4	/* read firmware revisions (7980xc) */
#define MTT_LOG		5	/* read diagnostic log */
#define MTT_FUP		6	/* read firmware update */
#define MTT_XSTAT	15	/* read extended status (7979a/7980a) */
#define MTT_DSJ		16	/* read DSJ (device specified jump) */
#define MTT_RCRC	17	/* read CRC */
#define MTT_XTEST	29	/* read 7979a/7980 extended self test */
#define MTT_LOOP	30	/* read interface loopback */
#define MTT_TEST	31	/* read self test */

/*
 *	Tape commands
 */

#define MTTC_SEL0	0	/* Select Unit 0 (native protocol) */
#define MTTC_WRITE	5	/* Write Record */
#define MTTC_WFM	6	/* Write File Mark */
#define MTTC_WGAP	7	/* Write Gap */
#define MTTC_READ	8	/* Read record */
#define MTTC_FSR	9	/* forward space record */
#define MTTC_BSR	10	/* backward space record */
#define MTTC_FSF	11	/* forward space file */
#define MTTC_BSF	12	/* backward space file */
#define MTTC_REW	13	/* rewind */
#define MTTC_REWOFF	14	/* rewind and go offline */
#define MTTC_DC6250	15	/* set data compressed 6250 */
#define MTTC_6250	16	/* set 6250 bpi */
#define MTTC_1600	17	/* set 1600 bpi */
#define MTTC_800	18	/* set 800 bpi */
#define MTTC_NC6250	19	/* set non-compressed 6250 */
#define MTTC_STSTP	20	/* start/stop mode only */
#define MTTC_STRM	21	/* enable streaming */
#define MTTC_DIRM	22	/* disable immediate report mode */
#define MTTC_EIRM	23	/* enable immediate report mode */
#define MTTC_STAT	24	/* request status */
#define MTTC_RLD	25	/* remote load */
#define MTTC_RUNLD	26	/* remote unload */
#define MTTC_RON	28	/* remote online */
#define MTTC_DDC	30	/* disable data compression */
#define MTTC_EDC	31	/* enable data compression */

/*
 *	End Command Bits (of any interest)
 */
#define	MTE_COMPLETE	0x08	/* "marks the end of the report phase" */
#define	MTE_IDLE	0x04	/* enables parallel poll resp. for online */
#define	MTE_STOP	0x02	/* aborts transfer of "read" data */

#define	MTE_DSJ_FORCE	0x100	/* XXX During readDSJ, force a status fetch */


struct	mt_stat {
	u_char	m_stat[6];
};

/* sc_flags */
#define	MTF_OPEN	0x0001	/* drive is in use (single-access device) */
#define	MTF_EXISTS	0x0002	/* device was found at boot time */
#define	MTF_ALIVE	0x0004	/* drive actually talks to us */
#define	MTF_WRT		0x0008	/* last command was a WRITE */
#define	MTF_IO		0x0010	/* next interrupt should start I/O (DMA) */
#define	MTF_REW		0x0020	/* tape is rewinding - must wait for it */
#define	MTF_HITEOF	0x0040	/* last read or FSR hit EOF (file mark) */
#define	MTF_HITBOF	0x0080	/* last BSR hit EOF (file mark) */
#define	MTF_ATEOT	0x0100	/* tape hit EOT - can allow one forward op */
#define	MTF_PASTEOT	0x0200	/* tape is beyond EOT - force backward motion */
#define	MTF_DSJTIMEO	0x0400	/* timed out hpibrecv()ing DSJ - continue it */
#define	MTF_STATTIMEO	0x0800	/* timed out receiving STATUS - continue it */
#define	MTF_STATCONT	0x1000	/* STATTIMEO is continuable */

/* additional "mtcommand"s */
#define MTRESET		16	/* reset the thing from scratch */
#define MTSET800BPI	17	/* density select */
#define MTSET1600BPI	18
#define MTSET6250BPI	19
#define MTSET6250DC	20	/* (data compressed - MT7980ID only) */

/* status bytes */
#define sc_stat1	sc_stat.m_stat[0]
#define sc_stat2	sc_stat.m_stat[1]
#define sc_stat3	sc_stat.m_stat[2]
#define sc_stat4	sc_stat.m_stat[3]
#define sc_stat5	sc_stat.m_stat[4]
#define sc_stat6	sc_stat.m_stat[5]

/*
 *	Status Register definitions
 */

#define	SR1_EOF		0x80	/* positioned at File Mark */
#define	SR1_BOT		0x40	/* positioned at Beginning of Tape */
#define	SR1_EOT		0x20	/* positioned at End of Tape */
#define	SR1_SOFTERR	0x10	/* Recoverable Error has Occured */
#define	SR1_REJECT	0x08	/* HPIB Cmd rejected - Regs 4 & 5 have info */
#define	SR1_RO		0x04	/* No Write Ring */
#define	SR1_ERR		0x02	/* Unrecoverable Data error - Reg 5 has info */
#define	SR1_ONLINE	0x01	/* Drive Online (must be to do any operation) */

#define	SR2_6250	0x80	/* tape is 6250BPI */
#define	SR2_UNKDEN	0x40	/* non-blank tape is of unknown density */
#define	SR2_PARITY	0x20	/* internal bus data parity error detected */
#define	SR2_OVERRUN	0x10	/* data buffer overrun (not possible?) */
#define	SR2_RUNAWAY	0x08	/* during read, no data detected on tape */
#define	SR2_OPEN	0x04	/* tape door is open */
#define	SR2_LONGREC	0x02	/* large record support (32k@1600, 60K@6250,
				   otherwise, it's 16K at all densities) */
#define	SR2_IMMED	0x01	/* Immediate Response (for writes) enabled */

#define	SR3_1600	0x80	/* tape is 1600BPI */
#define	SR3_800		0x40	/* tape is 800BPI */
#define	SR3_POWERUP	0x20	/* power recently restored or Dev Clr done */
#define	SR3_HPIBPAR	0x10	/* HPIB command parity error detected */
#define	SR3_LOST	0x08	/* position on tape is unknown */
#define	SR3_FMTERR	0x04	/* formatter error - Reg 5 has info */
#define	SR3_SVOERR	0x02	/* motion servo error - Reg 4 has info */
#define	SR3_CTLERR	0x01	/* controller error - Reg 5 has info */

#define SR4_ERCLMASK	0xe0	/* Mask of error classes (for SR1_REJECT) */
#define SR4_NONE	0x00
#define SR4_DEVICE	0x40
#define SR4_PROTOCOL	0x60
#define SR4_SELFTEST	0xe0
#define	SR4_RETRYMASK	0x1f	/* Mask for retry count (for any error) */

/* SR5 holds lots of error codes, referenced above.  Complete list:
 * (DEVICE REJECT)
 *	  5	Tape is write protected
 *	  6	Tape isn't loaded
 *	  7	Requested density not supported
 *	  9	Tape being read is unreadable
 *	 10	Tape being written is unidentifiable
 *	 11	Drive offline
 *	 16	Changing density while not at BOT
 *	 19	Backward motion requested while at BOT
 *	 23	Protocol out of sync
 *	 24	Unknown tape command
 *	 31	Write request too big for drive/density
 *	 32	Beyond EOT
 *	 33	Self Test Failure
 *	 37	Tape positioning failure while removing readaheads
 *	 40	Door open
 * (UNRECOVERED DATA/FORMAT ERRORS)
 *	 41	Tape velocity out of spec
 *	 45	Multiple track data error
 *	 47	Write verify failed
 *	 48	Noise found while trying to detect data record
 *	 49	Data format error
 *	 50	Couldn't identify tape after rewind
 *	 51	Gap detected before end of data record
 *	 52	Data block dropout
 *	 53	CRC error
 *	 54	Parity error
 *	 55	Door open
 *	 57	Maximum skew exceeded
 *	 58	False data block detected
 *	 59	Corrected data error on write
 *	 60	Buffer overrun - record size on tape larger than supported
 *	 61	Data block timeout (possibly record length too long)
 *	 62	Tape mark dropout
 *	 63	Tape mark unverified
 *	 64	Tape mark timeout (no gap following tape mark)
 * (POSITION or SERVO ERRORS) - these are ALL internal to tape drive
 *	 81	Servo unresponsive
 *	 82	Servo didn't respond with corect state
 *	 83	Servo shutdown
 *	 84	Servo detected hardware failure
 *	 85	Servo protocol error
 *	 86	Runtime Servo error
 *	 87	Missing position interrupt
 *	 88	No Gap after read or write
 *	 89	Motor shutdown for safety reasons
 *	 90	Couldn't find tape BOT mark
 *	 91	Drive motor running too fast or slow
 *	 92	Requested controller state invalid within context
 *	 94	Tape positioning failure
 * (FORMATTER ERROR)
 *	101,108	Read formatter unresponsive
 *	102,107	Read formatter hardware error
 *	103	Write detected bad block
 *	104	Erase failure
 *	105	No data detected after write
 *	106	Tracks out of sync on write verify
 *	109	No gap timeout
 *	110	Formatter <--> data buffer byte count mismatch
 * (CONTROLLER ERROR) - these are ALL internal to drive
 *	121	Transaction ID mismatch (device vs. controller)
 *	122	Devoce report has no coorepinding command
 *	123	Invalid device report
 *	124	Repost queue overflow
 *	125	Unknown command from device
 *	126	Command queue overflow
 *	128	Missing End-Of-Record flag in data buffer
 *	129	Data buffer parity error
 *	130	Data buffer underrun during write
 *	131	Byte count mismatch in data buffer queue
 *	132	Bad message type from device
 *	133	Abort between HPIB interface and channel program
 *	134	Unknown HPIB interface exception
 *	137	Illegal access to servo conntroller registers
 *	138	Device program firmware error
 *	139	Hardware utilities firmware error
 *	140	Channel program firmware error
 *	141	Encoder inoperative
 *	150	Tape position synchronization error
 *	151	Tape deblocking error (Xtra Capacity only)
 *	152	Compression/Decompression hardware error (Xtra Capacity only)
 * (PROTOCOL ERROR) - USUALLY indicates deficiency in driver
 *	161	No room in Command Queue
 *	162	Expected "request DSJ"
 *	163	Expected status request
 *	165	Unknown unit select
 *	166	Tape command secondary expected
 *	167	Data byte expected
 *	168	Missing EOI on data byte
 *	170	Write command phase protocol error
 *	172	Read record report phase error
 *	173	Report phase protocol error
 *	174	Cold load sequence error
 *	175	HPIB protocol sequence error
 *	176	END command expected
 *	178	END DATA expected
 *	180	Unknown interface secondary command
 *	181	Misplaced data byte
 *	184	Interface Loopback protocol error
 *	185	Self test protocol error
 *	188	HPIB parity error
 *	189	Operator reset during protocol sequence
 *	190	Device clear received
 */

/* SR6 is count of commands accepted since Immediate Response command failed */
