/*	$NetBSD: exec_ecoff.h,v 1.7 1995/06/16 02:07:56 mellon Exp $	*/

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
 *      This product includes software developed by Adam Glass.
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

#ifndef	_SYS_EXEC_ECOFF_H_
#define	_SYS_EXEC_ECOFF_H_

#include <machine/ecoff.h>

struct ecoff_filehdr {
	u_short ef_magic;	/* magic number */
	u_short ef_nsecs;	/* # of sections */
	u_int   ef_timestamp;	/* time and date stamp */
	u_long  ef_symptr;	/* file offset of symbol table */
	u_int   ef_syms;	/* # of symbol table entries */
	u_short ef_opthdr;	/* sizeof the optional header */
	u_short ef_flags;	/* flags??? */
};

struct ecoff_aouthdr {
	u_short ea_magic;
	u_short ea_vstamp;
	ECOFF_PAD
	u_long  ea_tsize;
	u_long  ea_dsize;
	u_long  ea_bsize;
	u_long  ea_entry;
	u_long  ea_text_start;
	u_long  ea_data_start;
	u_long  ea_bss_start;
	ECOFF_MACHDEP;
};

struct ecoff_scnhdr {		/* needed for size info */
	char	es_name[8];	/* name */
	u_long  es_physaddr;	/* physical addr? for ROMing?*/
	u_long  es_virtaddr;	/* virtual addr? */
	u_long  es_size;	/* size */
	u_long  es_raw_offset;	/* file offset of raw data */
	u_long  es_reloc_offset; /* file offset of reloc data */
	u_long  es_line_offset;	/* file offset of line data */
	u_short es_nreloc;	/* # of relocation entries */
	u_short es_nline;	/* # of line entries */
	u_int   es_flags;	/* flags */
};

#define ECOFF_HDR_SIZE (sizeof(struct ecoff_filehdr) + \
			sizeof(struct ecoff_aouthdr))

#define ECOFF_OMAGIC 0407
#define ECOFF_NMAGIC 0410
#define ECOFF_ZMAGIC 0413

#define ECOFF_ROUND(value, by) \
        (((value) + by - 1) & ~(by - 1))

#define ECOFF_BLOCK_ALIGN(eap, value) \
        ((eap)->ea_magic == ECOFF_ZMAGIC ? ECOFF_ROUND(value, ECOFF_LDPGSZ) : \
	 value)

#define ECOFF_TXTOFF(efp, eap) \
        ((eap)->ea_magic == ECOFF_ZMAGIC ? 0 : \
	 ECOFF_ROUND(ECOFF_HDR_SIZE + (efp)->ef_nsecs * \
		     sizeof(struct ecoff_scnhdr),ECOFF_SEGMENT_ALIGNMENT(eap)))

#define ECOFF_DATOFF(efp, eap) \
        (ECOFF_BLOCK_ALIGN(eap, ECOFF_TXTOFF(efp, eap) + (eap)->ea_tsize))

#define ECOFF_SEGMENT_ALIGN(eap, value) \
        (ECOFF_ROUND(value, ((eap)->ea_magic == ECOFF_ZMAGIC ? ECOFF_LDPGSZ : \
         ECOFF_SEGMENT_ALIGNMENT(eap))))

#ifdef _KERNEL
int	exec_ecoff_makecmds __P((struct proc *, struct exec_package *));
#endif /* _KERNEL */
#endif /* !_SYS_EXEC_ECOFF_H_ */
