/*	$NetBSD: video.h,v 1.2.2.1 1995/11/16 20:30:13 leo Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman.
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
 *      This product includes software developed by Leo Weppelman.
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

#ifndef _MACHINE_VIDEO_H
#define _MACHINE_VIDEO_H
/*
 * Access to circuitry for video
 */

#define	VIDEO	((struct video *)AD_VIDEO)

struct video {
    volatile	char	vdb[14];	/* sparsely filled		    */
    volatile	u_short vd_line_wide;	/* Falcon line word distance        */
    volatile	u_short vd_vert_wrap;	/* Falcon line length		    */
    volatile	char	vd_fill0[45];	/* filler			    */
    volatile	u_short	vd_st_rgb[16];	/* RGB for simultaneous colors	(ST)*/
    volatile	u_char	vd_st_res;	/* ST resolution		    */
    volatile	char	vd_fill1;	/* filler			    */
    volatile	u_short	vd_tt_res;	/* TT-resultion mode		    */
    volatile	u_char  vd_ste_hscroll;	/* MEGA STe hor bitwise scroll	    */
    volatile	u_short vd_fal_res;	/* Falcon resolution		    */
    volatile	char    vd_fill2[23];
    volatile	u_short vd_h_hold_cnt;	/* Falcon horizontal hold counter   */
    volatile	u_short vd_h_hold_tim;	/* Falcon horizontal hold timer     */
    volatile	u_short vd_h_bord_beg;  /* Falcon horizontal border begin   */
    volatile	u_short vd_h_bord_end;	/* Falcon horizontal border end     */
    volatile	u_short vd_h_dis_beg;	/* Falcon horizontal display begin  */
    volatile	u_short vd_h_dis_end;	/* Falcon horizontal display end    */
    volatile	u_short vd_h_ss;	/* Falcon horizontal SS             */
    volatile    u_short vd_h_fs;	/* Falcon horizontal FS		    */
    volatile	u_short vd_h_hh;	/* Falcon horizontal HH		    */
    volatile	char    vd_fill3[13];
    volatile	u_short vd_v_freq_cnt;	/* Falcon vertical frequency count  */
    volatile	u_short vd_v_freq_tim;	/* Falcon vertical frequency timer  */
    volatile	u_short vd_v_bord_beg;	/* Falcon vertical border begin     */
    volatile	u_short vd_v_bord_end;	/* Falcon vertical border end       */
    volatile	u_short vd_v_dis_beg;	/* Falcon vertical display begin    */
    volatile	u_short vd_v_dis_end;	/* Falcon vertical display end      */
    volatile	u_short vd_v_ss;	/* Falcon vertical SS               */
    volatile	char    vd_fill4[17];
    volatile	u_short vd_unknown;	/* Falcon, purpose unknown          */
    volatile	u_short vd_fal_ctrl;	/* Falcon video control		    */
    volatile	char    vd_fill5[315];
    volatile	u_short vd_tt_rgb[256];	/* RGB for simultaneous TT colors   */
    volatile	char	vd_fill6[4608];
    volatile	u_long  vd_fal_rgb[256];/* RGB for Falcon colors            */
};

#define	vd_ramh		vdb[ 1]	/* base address Video RAM, high byte	*/
#define	vd_ramm		vdb[ 3]	/* base address Video RAM, mid byte	*/
#define	vd_raml		vdb[13]	/* base address Video RAM, low byte	*/
#define	vd_ptrh		vdb[ 5]	/* scan address Video RAM, high byte	*/
#define	vd_ptrm		vdb[ 7]	/* scan address Video RAM, mid byte	*/
#define	vd_ptrl		vdb[ 9]	/* scan address Video RAM, low byte	*/
#define	vd_sync		vdb[10]	/* synchronization mode			*/

/* bits in vd_sync: */
#define	SYNC_EXT	0x01	/* extern sync				*/
#define	SYNC_50		0x02	/* 50 Hertz (used for color)		*/

/* bits in vd_st_rgb[]: */
#define	RGB_B		0x0007
#define	RGB_G		0x0070
#define	RGB_R		0x0700

/* some values for vd_st_rgb[]: */
#define	RGB_BLACK	0x0000
#define	RGB_RED		0x0700
#define	RGB_GREEN	0x0070
#define	RGB_BLUE	0x0007
#define	RGB_WHITE	0x0777
#define	RGB_MAGENTA	0x0707
#define	RGB_CYAN	0x0077
#define	RGB_YELLOW	0x0770
#define	RGB_LGREY	0x0555
#define	RGB_DGREY	0x0222

/* values for vd_st_res: */
#define	RES_LOW		0x00	/* 320x200, 16 colors			*/
#define	RES_MID		0x01	/* 640x200,  4 colors			*/
#define	RES_HIGH	0x02	/* 640x400, monochrome			*/

/* masks for vd_tt_res: */
#define	RES_STLOW	0x0000	/* 320x200, 16 colors			*/
#define	RES_STMID	0x0100	/* 640x200,  4 colors			*/
#define	RES_STHIGH	0x0200	/* 640x400, monochrome			*/
#define	RES_TTMID	0x0400	/* 640x480, 16 colors			*/
#define	RES_TTHIGH	0x0600	/* 1280x960, monochrome			*/
#define	RES_TTLOW	0x0700	/* 320x480,  256 colors			*/
#define TT_PALLET	0x000f	/* Pallette number			*/
#define	TT_HYMONO	0x8000	/* Hyper mono mode			*/
#define	TT_SHOLD	0x1000	/* Sample/hold mode			*/

/* The falcon video modes */
#define RES_FALAUTO	0	/* Falcon resolution dedected at boot	*/
#define RES_VGA2	1	/* 640x480,   2 colors			*/
#define RES_VGA4	2	/* 640x480,   4 colors			*/
#define RES_VGA16	3	/* 640x480,  16 colors			*/
#define RES_VGA256	4	/* 640x480, 256 colors			*/
#define RES_DIRECT	5	/* 320x200, 65536 colors		*/
#define RES_FAL_STLOW	6	/* 320x200,  16 colors			*/
#define RES_FAL_STMID	7	/* 640x200,   4 colors			*/
#define RES_FAL_STHIGH	8	/* 640x400,   2 colors			*/
#define RES_FAL_TTLOW	9	/* 320x480, 256 colors			*/

/*
 * Yahama YM-2149 Programmable Sound Generator
 */

#define	SOUND	((struct sound *)AD_SOUND)

struct sound {
	char	sdb[4];		/* use only the even bytes		*/
};

#define	sd_selr		sdb[0]	/* select register			*/
#define	sd_rdat		sdb[0]	/* read register data			*/
#define	sd_wdat		sdb[2]	/* write register data			*/

/*
 * Accessing the YM-2149 registers is indirect through ST-specific
 * circuitry by writing the register number into sd_selr.
 */
#define	YM_PA0		0	/* Period Channel A, bits 0-7		*/
#define	YM_PA1		1	/* Period Channel A, bits 8-11		*/
#define	YM_PB0		2	/* Period Channel B, bits 0-7		*/
#define	YM_PB1		3	/* Period Channel B, bits 8-11		*/
#define	YM_PC0		4	/* Period Channel C, bits 0-7		*/
#define	YM_PC1		5	/* Period Channel C, bits 8-11		*/
#define	YM_PNG		6	/* Period Noise Generator, bits 0-4	*/
#define	YM_MFR		7	/* Multi Function Register		*/
#define	YM_VA		8	/* Volume Channel A			*/
#define	YM_VB		9	/* Volume Channel B			*/
#define	YM_VC		10	/* Volume Channel C			*/
#define	YM_PE0		11	/* Period Envelope, bits 0-7		*/
#define	YM_PE1		12	/* Period Envelope, bits 8-15		*/
#define	YM_WFE		13	/* Wave Form Envelope			*/
#define	YM_IOA		14	/* I/O port A				*/
#define	YM_IOB		15	/* I/O port B				*/

/* bits in MFR: */
#define	SA_OFF		0x01	/* Sound Channel A off			*/
#define	SB_OFF		0x02	/* Sound Channel B off			*/
#define	SC_OFF		0x04	/* Sound Channel C off			*/
#define	NA_OFF		0x08	/* Noise Channel A off			*/
#define	NB_OFF		0x10	/* Noise Channel B off			*/
#define	NC_OFF		0x20	/* Noise Channel C off			*/
#define	PA_OUT		0x40	/* Port A for Output			*/
#define	PB_OUT		0x80	/* Port B for Output			*/

/* bits in Vx: */
#define	VOLUME		0x0F	/* 16 steps				*/
#define	ENVELOP		0x10	/* volume steered by envelope		*/

/* bits in WFE: */
#define	WF_HOLD		0x01	/* hold after one period		*/
#define	WF_ALTERNAT	0x02	/* up and down (no saw teeth)		*/
#define	WF_ATTACK	0x04	/* start up				*/
#define	WF_CONTINUE	0x08	/* multiple periods			*/

/* names for bits in Port A (ST specific): */
#define	PA_SIDEB	0x01	/* select floppy head - if double sided	*/
#define	PA_FLOP0	0x02	/* Drive Select Floppy 0		*/
#define	PA_FLOP1	0x04	/* Drive Select Floppy 1		*/
#define	PA_SRTS		0x08	/* Serial RTS				*/
#define	PA_SDTR		0x10	/* Serial DTR				*/
#define	PA_PSTROBE	0x20	/* Parallel Strobe			*/
#define	PA_USER		0x40	/* Free Pin on Monitor Connector	*/
#define	PA_SER2		0x80	/* Choose between LAN or Ser2 port	*/

#endif /*  _MACHINE_VIDEO_H */
