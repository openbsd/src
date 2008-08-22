/* $OpenBSD: ecoff_hide.c,v 1.1 2008/08/22 15:18:55 deraadt Exp $	 */

/*-
 * Copyright (c) 1997 Niklas Hallqvist.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <sys/types.h>
#include <sys/exec.h>
#ifdef _NLIST_DO_ECOFF
#include <sys/exec_ecoff.h>
#include <string.h>

/* Do we have these symbols in any include file?  */
#define scText		1
#define scData		2
#define scBss		3
#define scSData		13
#define scSBss		14
#define scRData		15

#define stNil		0

extern int      in_keep_list(char *);

void
ecoff_hide(int fd, char *p)
{
	struct ecoff_exechdr *ehdr = (struct ecoff_exechdr *) p;
	struct ecoff_symhdr *shdr = (struct ecoff_symhdr *) (p + ehdr->f.f_symptr);
	u_int           ecnt = shdr->esymMax;
	struct ecoff_extsym *esym = (struct ecoff_extsym *) (p + shdr->cbExtOffset);
	char           *estr = p + shdr->cbSsExtOffset;
	int             i;

	for (i = 0; i < ecnt; i++, esym++)
		if ((esym->es_class == scText || esym->es_class == scData ||
		    esym->es_class == scBss || esym->es_class == scSData ||
		    esym->es_class == scSBss || esym->es_class == scRData) &&
		    !in_keep_list(estr + esym->es_strindex))
			esym->es_type = stNil;
}
#endif				/* _NLIST_DO_ECOFF */
