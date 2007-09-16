/* $OpenBSD: node_int.h,v 1.1 2007/09/16 10:20:17 espie Exp $ */

/*
 * Copyright (c) 2007 Marc Espie.
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


/* List of all nodes recognized by the make parser */
#define NODE_DEFAULT 	".DEFAULT"
#define NODE_EXEC	".EXEC"
#define NODE_IGNORE	".IGNORE"
#define NODE_INCLUDES	".INCLUDES"
#define NODE_INVISIBLE	".INVISIBLE"
#define NODE_JOIN	".JOIN"
#define NODE_LIBS	".LIBS"
#define NODE_MADE	".MADE"
#define NODE_MAIN	".MAIN"
#define NODE_MAKE	".MAKE"
#define NODE_MAKEFLAGS	".MAKEFLAGS"
#define NODE_MFLAGS	".MFLAGS"
#define NODE_NOTMAIN	".NOTMAIN"
#define NODE_NOTPARALLEL	".NOTPARALLEL"
#define NODE_NO_PARALLEL	".NOPARALLEL"
#define NODE_NULL	".NULL"
#define NODE_OPTIONAL	".OPTIONAL"
#define NODE_ORDER	".ORDER"
#define NODE_PARALLEL	".PARALLEL"
#define NODE_PATH	".PATH"
#define NODE_PHONY	".PHONY"
#define NODE_PRECIOUS	".PRECIOUS"
#define NODE_RECURSIVE	".RECURSIVE"
#define NODE_SILENT	".SILENT"
#define NODE_SINGLESHELL	".SINGLESHELL"
#define NODE_SUFFIXES	".SUFFIXES"
#define NODE_USE	".USE"
#define NODE_WAIT	".WAIT"

#define NODE_BEGIN	".BEGIN"
#define NODE_END	".END"
#define NODE_INTERRUPT	".INTERRUPT"
