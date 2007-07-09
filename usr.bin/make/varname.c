/*	$OpenPackages$ */
/*	$OpenBSD: varname.c,v 1.3 2007/07/09 12:29:45 espie Exp $	*/
/*
 * Copyright (c) 2001 Marc Espie.
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
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include "config.h"
#include "defines.h"
#include "var.h"
#include "buf.h"
#include "varname.h"

const char *
VarName_Get(const char *start, struct Name *name, SymTable *ctxt, bool err, const char *(*cont)(const char *))
{
	const char *p;
	size_t len;

	p = cont(start);
	/* If we don't want recursive variables, we skip over '$' */
	if (!FEATURES(FEATURE_RECVARS)) {
		while (*p == '$')
			p = cont(p);
	}
	if (*p != '$') {
		name->s = start;
		name->e = p;
		name->tofree = false;
		return p;
	} else {
		BUFFER buf;
		Buf_Init(&buf, MAKE_BSIZE);
		for (;;) {
			Buf_Addi(&buf, start, p);
			if (*p != '$') {
				name->s = (const char *)Buf_Retrieve(&buf);
				name->e = name->s + Buf_Size(&buf);
				name->tofree = true;
				return p;
			}
			start = p;
			Var_ParseBuffer(&buf, start, ctxt, err, &len);
			start += len;
			p = cont(start);
		}
	}
}

void
VarName_Free(struct Name *name)
{
	if (name->tofree)
		free((char *)name->s);
}

