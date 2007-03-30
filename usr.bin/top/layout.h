/*	$OpenBSD: layout.h,v 1.5 2007/03/30 19:21:19 otto Exp $	*/

/*
 *  Top users/processes display for Unix
 *  Version 3
 *
 * Copyright (c) 1984, 1989, William LeFebvre, Rice University
 * Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
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
 * IN NO EVENT SHALL THE AUTHOR OR HIS EMPLOYER BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  Top - a top users display for Berkeley Unix
 *
 *  This file defines the locations on the screen for various parts of the
 *  display.  These definitions are used by the routines in "display.c" for
 *  cursor addressing.
 */

#define  x_lastpid	10
#define  y_lastpid	0
#define  x_loadave	33
#define  x_loadave_nompid	15
#define  y_loadave	0
#define  x_procstate	0
#define  y_procstate	1
#define  x_brkdn	15
#define  y_brkdn	1
#define  x_mem		8
extern int y_mem;
extern int y_message;
extern int y_header;
#define  x_idlecursor	0
extern int y_idlecursor;
extern int y_procs;

#define  y_cpustates	2
