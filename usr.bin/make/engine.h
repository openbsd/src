#ifndef ENGINE_H
#define ENGINE_H
/*	$OpenBSD: engine.h,v 1.3 2007/11/02 17:27:24 espie Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)job.h 8.1 (Berkeley) 6/6/93
 */

/* ok = Job_CheckCommands(node, abort);
 *	verify the integrity of a node's commands, pulling stuff off
 * 	.DEFAULT and other places if necessary.
 */
extern bool Job_CheckCommands(GNode *, void (*abortProc)(char *, ...));
/* Job_Touch(node, besilent);
 *	touch the path corresponding to a node or update the corresponding
 *	archive object.
 */
extern void Job_Touch(GNode *, bool);
/* Make_TimeStamp(parent, child);
 *	ensure parent is at least as recent as child.
 */
extern void Make_TimeStamp(GNode *, GNode *);
/* Make_HandleUse(user_node, usee_node);
 *	let user_node inherit the commands from usee_node
 */
extern void Make_HandleUse(GNode *, GNode *);

/* old = Make_OODate(node);
 *	check if a given node is out-of-date.
 */
extern bool Make_OODate(GNode *);
/* Make_DoAllVar(node);
 *	fill all dynamic variables for a node.
 */
extern void Make_DoAllVar(GNode *);
extern volatile sig_atomic_t got_signal;

extern volatile sig_atomic_t got_SIGINT, got_SIGHUP, got_SIGQUIT,
    got_SIGTERM, got_SIGTSTP, got_SIGTTOU, got_SIGTTIN, got_SIGWINCH;

extern void SigHandler(int);
extern int run_gnode(GNode *, int);

extern void setup_engine(void);

#endif
