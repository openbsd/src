/*
 * Keyboard definitions
 *
 *	$Id: kbdreg.h,v 1.1.1.1 1995/10/18 10:39:15 deraadt Exp $
 */

#define	KBSTATP		(PICA_SYS_KBD + 0x61)	/* controller status port (I) */
#define	 KBS_DIB	0x01	/* data in buffer */
#define	 KBS_IBF	0x02	/* input buffer low */
#define	 KBS_WARM	0x04	/* input buffer low */
#define	 KBS_OCMD	0x08	/* output buffer has command */
#define	 KBS_NOSEC	0x10	/* security lock not engaged */
#define	 KBS_TERR	0x20	/* transmission error */
#define	 KBS_RERR	0x40	/* receive error */
#define	 KBS_PERR	0x80	/* parity error */

#define	KBCMDP		(PICA_SYS_KBD + 0x61)	/* controller port (O) */
#define	KBDATAP		(PICA_SYS_KBD + 0x60)	/* data port (I) */
#define	KBOUTP		(PICA_SYS_KBD + 0x60)	/* data port (O) */

#define	K_RDCMDBYTE	0x20
#define	K_LDCMDBYTE	0x60

#define	KC8_TRANS	0x40	/* convert to old scan codes */
#define	KC8_MDISABLE	0x20	/* disable mouse */
#define	KC8_KDISABLE	0x10	/* disable keyboard */
#define	KC8_IGNSEC	0x08	/* ignore security lock */
#define	KC8_CPU		0x04	/* exit from protected mode reset */
#define	KC8_MENABLE	0x02	/* enable mouse interrupt */
#define	KC8_KENABLE	0x01	/* enable keyboard interrupt */
#define	CMDBYTE		(KC8_TRANS|KC8_CPU|KC8_MENABLE|KC8_KENABLE)

/* keyboard commands */
#define	KBC_RESET	0xFF	/* reset the keyboard */
#define	KBC_RESEND	0xFE	/* request the keyboard resend the last byte */
#define	KBC_SETDEFAULT	0xF6	/* resets keyboard to its power-on defaults */
#define	KBC_DISABLE	0xF5	/* as per KBC_SETDEFAULT, but also disable key scanning */
#define	KBC_ENABLE	0xF4	/* enable key scanning */
#define	KBC_TYPEMATIC	0xF3	/* set typematic rate and delay */
#define	KBC_SETTABLE	0xF0	/* set scancode translation table */
#define	KBC_MODEIND	0xED	/* set mode indicators (i.e. LEDs) */
#define	KBC_ECHO	0xEE	/* request an echo from the keyboard */

/* keyboard responses */
#define	KBR_EXTENDED	0xE0	/* extended key sequence */
#define	KBR_RESEND	0xFE	/* needs resend of command */
#define	KBR_ACK		0xFA	/* received a valid command */
#define	KBR_OVERRUN	0x00	/* flooded */
#define	KBR_FAILURE	0xFD	/* diagnosic failure */
#define	KBR_BREAK	0xF0	/* break code prefix - sent on key release */
#define	KBR_RSTDONE	0xAA	/* reset complete */
#define	KBR_ECHO	0xEE	/* echo response */
