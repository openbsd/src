/*	$OpenBSD: ecoff_machdep.h,v 1.4 2007/10/16 15:55:33 deraadt Exp $	*/
/*	$NetBSD: ecoff_machdep.h,v 1.3 1996/05/09 23:47:25 cgd Exp $	*/

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

#define ECOFF_LDPGSZ 4096

#define ECOFF_PAD \
	u_short	bldrev;					/* XXX */

#define ECOFF_MACHDEP \
        u_int	gprmask; \
        u_int	fprmask; \
        u_long	gp_value

#define ECOFF_MAGIC_ALPHA		0603
#define ECOFF_MAGIC_NATIVE_ALPHA	0605
#define ECOFF_BADMAG(ep)						\
	((ep)->f.f_magic != ECOFF_MAGIC_ALPHA &&			\
	    (ep)->f.f_magic != ECOFF_MAGIC_NATIVE_ALPHA)

#define ECOFF_FLAG_EXEC			0002
#define ECOFF_SEGMENT_ALIGNMENT(ep) \
    (((ep)->f.f_flags & ECOFF_FLAG_EXEC) == 0 ? 8 : 16)

#define	ECOFF_FLAG_OBJECT_TYPE_MASK	0x3000
#define	ECOFF_OBJECT_TYPE_NO_SHARED	0x1000
#define	ECOFF_OBJECT_TYPE_SHARABLE	0x2000
#define	ECOFF_OBJECT_TYPE_CALL_SHARED	0x3000

struct ecoff_symhdr {
	int16_t		magic;
	int16_t		vstamp;
	int32_t		lineMax;
	int32_t		densenumMax;
	int32_t		procMax;
	int32_t		lsymMax;
	int32_t		optsymMax;
	int32_t		auxsymMax;
	int32_t		lstrMax;
	int32_t		estrMax;
	int32_t		fdMax;
	int32_t		rfdMax;
	int32_t		esymMax;
	long		linesize;
	long		cbLineOffset;
	long		cbDnOffset;
	long		cbPdOffset;
	long		cbSymOffset;
	long		cbOptOffset;
	long		cbAuxOffset;
	long		cbSsOffset;
	long		cbSsExtOffset;
	long		cbFdOffset;
	long		cbRfdOffset;
	long		cbExtOffset;
};

struct ecoff_extsym {
	long		es_value;
	int		es_strindex;
	unsigned int	es_type:6;
	unsigned int	es_class:5;
	unsigned int	:1;
	unsigned int	es_symauxindex:20;
	unsigned int	es_jmptbl:1;
	unsigned int	es_cmain:1;
	unsigned int	es_weakext:1;
	unsigned int	:29;
	int		es_indexfld;
};

#ifdef _KERNEL
void cpu_exec_ecoff_setregs(struct proc *, struct exec_package *, u_long, register_t *);
#endif
