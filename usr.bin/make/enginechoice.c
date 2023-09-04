/*	$OpenBSD: enginechoice.c,v 1.4 2023/09/04 11:35:11 espie Exp $ */
/*
 * Copyright (c) 2020 Marc Espie.
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
#include "defines.h"
#include "compat.h"
#include "make.h"
#include "enginechoice.h"

struct engine {
	void (*run_list)(Lst, bool *, bool *);
	void (*node_updated)(GNode *);
	void (*init)(void);
} 
	compat_engine = { Compat_Run, Compat_Update, Compat_Init }, 
	parallel_engine = { Make_Run, Make_Update, Make_Init }, 
	*engine;

void
choose_engine(bool compat)
{
	engine = compat ? &compat_engine: &parallel_engine;
	engine->init();
}

void
engine_run_list(Lst l, bool *has_errors, bool *out_of_date)
{
	engine->run_list(l, has_errors, out_of_date);
}

void
engine_node_updated(GNode *gn)
{
	engine->node_updated(gn);
}
