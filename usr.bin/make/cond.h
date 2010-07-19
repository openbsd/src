#ifndef COND_H
#define COND_H
/*	$OpenBSD: cond.h,v 1.5 2010/07/19 19:46:44 espie Exp $ */

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

/* cond
 *	Parse Makefile conditionals.
 */

/* Values returned by Cond_Eval.  */
#define COND_PARSE	0	/* Parse the next lines */
#define COND_SKIP	1	/* Skip the next lines */
#define COND_INVALID	2	/* Not a conditional statement */
#define COND_ISFOR	3
#define COND_ISUNDEF	4
#define COND_ISINCLUDE	5
#define COND_ISPOISON	6

/* whattodo = Cond_Eval(line);
 *	Parses a conditional expression (without the leading dot),
 *	and returns a decision value.
 *	State is kept internally, since conditionals nest.  */
extern int Cond_Eval(const char *);

/* Cond_End();
 *	To call at end of parsing, checks that all conditionals were
 *	closed.  */
extern void Cond_End(void);

#endif
