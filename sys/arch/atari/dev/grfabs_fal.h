/*	$NetBSD: grfabs_fal.h,v 1.1 1996/01/02 20:59:24 leo Exp $	*/

/*
 * Copyright (c) 1995 Thomas Gerner.
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
 *      This product includes software developed by Thomas Gerner
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

#ifndef _GRFABS_FAL_H
#define _GRFABS_FAL_H
/*
 * Struct to hold the values for the different video modes
 */


struct videl {
    u_short vm_reg;		/* video mode			    */
    u_char  vd_syncmode;	/* Syncronisation		    */
    u_short vd_line_wide;	/* Falcon line word distance        */
    u_short vd_vert_wrap;	/* Falcon line length		    */
    u_char  vd_st_res;		/* ST resolution		    */
    u_short vd_fal_res;		/* Falcon resolution		    */
    u_short vd_h_hold_tim;	/* Falcon horizontal hold timer     */
    u_short vd_h_bord_beg;  	/* Falcon horizontal border begin   */
    u_short vd_h_bord_end;	/* Falcon horizontal border end     */
    u_short vd_h_dis_beg;	/* Falcon horizontal display begin  */
    u_short vd_h_dis_end;	/* Falcon horizontal display end    */
    u_short vd_h_ss;		/* Falcon horizontal SS             */
    u_short vd_h_fs;		/* Falcon horizontal FS		    */
    u_short vd_h_hh;		/* Falcon horizontal HH		    */
    u_short vd_v_freq_tim;	/* Falcon vertical frequency timer  */
    u_short vd_v_bord_beg;	/* Falcon vertical border begin     */
    u_short vd_v_bord_end;	/* Falcon vertical border end       */
    u_short vd_v_dis_beg;	/* Falcon vertical display begin    */
    u_short vd_v_dis_end;	/* Falcon vertical display end      */
    u_short vd_v_ss;		/* Falcon vertical SS               */
    u_short vd_fal_ctrl;	/* Falcon video control		    */
    u_short vd_fal_mode;	/* Falcon video mode	            */
};

#endif /*  _GRFABS_FAL_H */
