/*	$OpenBSD: kbdreg.h,v 1.2 1999/01/27 04:46:05 imp Exp $	*/

/*
 * Copyright (c) 1996 Per Fogelstrom
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
 *      This product includes software developed by Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MIPS_KBDREG_H_
#define	_MIPS_KBDREG_H_

/*
 * Keyboard controller definitions
 */

#define	 KBS_DIB	0x01	/* data in buffer */
#define	 KBS_IBF	0x02	/* input buffer low */
#define	 KBS_WARM	0x04	/* input buffer low */
#define	 KBS_OCMD	0x08	/* output buffer has command */
#define	 KBS_NOSEC	0x10	/* security lock not engaged */
#define	 KBS_TERR	0x20	/* transmission error */
#define	 KBS_RERR	0x40	/* receive error */
#define	 KBS_PERR	0x80	/* parity error */

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


#ifdef _KERNEL

static __inline int      kb_output_wait __P((void));
static __inline int      kb_input_wait  __P((void));
static __inline void     kb_input_flush __P((void));
static __inline u_int8_t kb_get8042     __P((void));
static __inline int	 kb_put8042     __P((u_char));

static int kb_cmd_port;
static int kb_data_port;     

static __inline int
kb_output_wait()
{
	int	to = 100000;

	while(to--) {
		if((inb(kb_cmd_port) & KBS_IBF) == 0) {
			DELAY(10);
			return(1);
		}
	}
	return(0);
}

static __inline int
kb_input_wait()
{
	int	to = 100000;

	while(to--) {
		if((inb(kb_cmd_port) & KBS_DIB) == 0) {
			DELAY(10);
			return(1);
		}
	}
	return(0);
}

static __inline void
kb_input_flush()
{
	u_int c;

	while((c = inb(kb_cmd_port) & (KBS_DIB | KBS_IBF))) {
		if(c == KBS_DIB) {
			DELAY(10);
			c = inb(kb_data_port);
		}
	}
}

static __inline u_int8_t
kb_get8042()
{
	if(!kb_output_wait())
		return(0);
	outb(kb_cmd_port, K_LDCMDBYTE);
	if(!kb_input_wait())
		return(0);
	return(inb(kb_data_port));
}

static __inline int
kb_put8042(v)
	u_char v;
{
	if(!kb_output_wait())
		return(0);
	outb(kb_cmd_port, K_LDCMDBYTE);
	if(!kb_output_wait())
		return(0);
	outb(kb_data_port, v);
	return(1);
}


#endif /* _KERNEL */

#endif /* _MIPS_KBDREG_H_ */
