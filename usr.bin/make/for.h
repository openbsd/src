#ifndef FOR_H
#define FOR_H
/*	$OpenPackages$ */
/* $OpenBSD: for.h,v 1.3 2007/09/17 09:28:36 espie Exp $ */
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


/*
 * for
 *	Functions to handle loops in a makefile.
 */

struct For_;
typedef struct For_ For;
/* handle = For_Eval(line);
 *	Evaluate for loop in line, and returns an opaque handle.
 *	Loop lines are parsed as
 *	 <variable1> ... in <values>
 *	assuming .for has been parsed by previous modules.
 *	Returns NULL if this does not parse as a for loop after all.  */
extern For *For_Eval(const char *);

/* finished = For_Accumulate(handle, line);
 *	Accumulate lines in a loop, until we find the matching .endfor. */
extern bool For_Accumulate(For *, const char *);

/* For_Run(handle);
 *	Runs the complete for loop, pushing back expanded lines to reparse
 *	using Parse_FromString. */
extern void For_Run(For *);

#endif
