/*	$OpenBSD: qop_errlist.c,v 1.1.1.1 2001/06/27 18:23:22 kjc Exp $	*/
/*	$KAME: qop_errlist.c,v 1.2 2000/10/18 09:15:19 kjc Exp $	*/
/*
 * Copyright (C) 1999-2000
 *	Sony Computer Science Laboratories, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

const char *qop_errlist[] = {
	"no error",					/* 0 */
	"syscall error",				/* 1 */
	"no memory",					/* 2 */
	"invalid parameter",				/* 3 */
	"out of range",					/* 4 */
	"bad interface",				/* 5 */
	"bad class",					/* 6 */
	"bad filter",					/* 7 */
	"class error",					/* 8 */
	"bad class value",				/* 9 */
	"class operation not permitted",		/* 10 */
	"filter error",					/* 11 */
	"bad filter value",				/* 12 */
	"filter shadows an existing filter",		/* 13 */
	"admission failure",				/* 14 */
	"admission failure (no bandwidth)",		/* 15 */
	"admission failure (delay)",			/* 16 */
	"admission failure (no service)",		/* 17 */
	"policy error",					/* 18 */
};
