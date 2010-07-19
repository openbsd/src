#ifndef VAR_INT_H
#define VAR_INT_H
/* $OpenBSD: var_int.h,v 1.2 2010/07/19 19:46:44 espie Exp $ */
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

/* Special variable names for hashing */
#define TARGET		  "@"	/* Target of dependency */
#define OODATE		  "?"	/* All out-of-date sources */
#define ALLSRC		  ">"	/* All sources */
#define IMPSRC		  "<"	/* Source implied by transformation */
#define PREFIX		  "*"	/* Common prefix */
#define ARCHIVE 	  "!"	/* Archive in "archive(member)" syntax */
#define MEMBER		  "%"	/* Member in "archive(member)" syntax */
#define LONGTARGET	".TARGET"
#define LONGOODATE	".OODATE"
#define LONGALLSRC	".ALLSRC"
#define LONGIMPSRC	".IMPSRC"
#define LONGPREFIX	".PREFIX"
#define LONGARCHIVE	".ARCHIVE"
#define LONGMEMBER	".MEMBER"

/* System V   extended variables (get directory/file part) */
#define FTARGET		"@F"
#define DTARGET		"@D"
#define FIMPSRC		"<F"
#define DIMPSRC		"<D"
#define FPREFIX		"*F"
#define DPREFIX		"*D"
#define FARCHIVE	"!F"
#define DARCHIVE	"!D"
#define FMEMBER		"%F"
#define DMEMBER		"%D"

#endif
