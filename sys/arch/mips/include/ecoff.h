/*	$OpenBSD: ecoff.h,v 1.2 1999/01/27 04:46:05 imp Exp $	*/
/*	$NetBSD: ecoff.h,v 1.4 1995/06/16 02:07:33 mellon Exp $	*/

/*
 * Copyright (c) 1994 Adam Glass
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
 *	This product includes software developed by Adam Glass.
 * 4. The name of the Author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Adam Glass ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Adam Glass BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _MIPS_ECOFF_H_
#define _MIPS_ECOFF_H_

#define ECOFF_LDPGSZ 4096

#define ECOFF_PAD

#define ECOFF_MACHDEP \
        u_long ea_gprmask; \
        u_long ea_cprmask[4]; \
        u_long ea_gp_value

#define ECOFF_MAGIC_MIPSEL 0x0162
#define ECOFF_BADMAG(ex) ((ex)->f.f_magic != ECOFF_MAGIC_MIPSEL)

#define ECOFF_SEGMENT_ALIGNMENT(ep) ((ep)->a.vstamp < 23 ? 8 : 16)

struct ecoff_symhdr {
        int16_t         sh_magic;
        int16_t         sh_vstamp;
        int32_t         sh_linemax;
        int32_t         sh_densenummax;
        int32_t         sh_procmax;
        int32_t         sh_lsymmax;
        int32_t         sh_optsymmax;
        int32_t         sh_auxxymmax;
        int32_t         sh_lstrmax;
        int32_t         sh_estrmax;
        int32_t         sh_fdmax;
        int32_t         sh_rfdmax;
        int32_t         sh_esymmax;
        long            sh_linesize;
        long            sh_lineoff;
        long            sh_densenumoff;
        long            sh_procoff; 
        long            sh_lsymoff;
        long            sh_optsymoff;
        long            sh_auxsymoff;
        long            sh_lstroff;
        long            sh_estroff;
        long            sh_fdoff;
        long            sh_rfdoff;
        long            sh_esymoff;
};
/* Some day they will make up their minds.... */
#define	esymMax		sh_esymmax
#define cbExtOffset	sh_esymoff
#define	cbSsExtOffset	sh_estroff

struct ecoff_extsym {  
        long            es_value;
        int             es_strindex;
        unsigned        es_type:6;
        unsigned        es_class:5;
        unsigned        :1;
        unsigned        es_symauxindex:20;  
        unsigned        es_jmptbl:1;
        unsigned        es_cmain:1;
        unsigned        es_weakext:1;
        unsigned        :29;
        int             es_indexfld; 
};

#endif /* !_MIPS_ECOFF_H_ */
