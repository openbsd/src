/*	$OpenBSD: init.c,v 1.6 2010/07/19 19:46:44 espie Exp $ */

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
#include <stdio.h>
#include "defines.h"
#include "config.h"
#include "init.h"
#include "timestamp.h"
#include "stats.h"
#include "dir.h"
#include "parse.h"
#include "var.h"
#include "arch.h"
#include "targ.h"
#include "suff.h"
#include "job.h"

void
Init(void)
{
	Init_Timestamp();
	Init_Stats();
	Targ_Init();
	Dir_Init();		/* Initialize directory structures so -I flags
				 * can be processed correctly */
	Parse_Init();		/* Need to initialize the paths of #include
				 * directories */
	Var_Init();		/* As well as the lists of variables for
				 * parsing arguments */
	Arch_Init();
	Suff_Init();
}

#ifdef CLEANUP
void
End(void)
{
	Suff_End();
	Targ_End();
	Arch_End();
	Var_End();
	Parse_End();
	Dir_End();
	Job_End();
}
#endif
