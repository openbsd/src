/*	$OpenBSD: dhctoken.h,v 1.5 2006/05/15 08:10:57 fkr Exp $	*/

/* Tokens for config file lexer and parser. */

/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999
 * The Internet Software Consortium.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#define TOK_FIRST_TOKEN	TOK_HARDWARE
#define TOK_HARDWARE		257
#define TOK_FILENAME		258
#define TOK_FIXED_ADDR		259
#define TOK_OPTION		260
#define TOK_ETHERNET		261
#define TOK_STRING		262
#define TOK_NUMBER		263
#define TOK_NUMBER_OR_NAME	264
#define TOK_NAME		265
#define TOK_LEASE		266
#define TOK_SERVER_NAME		267
#define TOK_TOKEN_RING		268
#define TOK_SEND		269
#define TOK_REQUEST		270
#define TOK_REQUIRE		271
#define TOK_TIMEOUT		272
#define TOK_RETRY		273
#define TOK_SELECT_TIMEOUT	274
#define TOK_SCRIPT		275
#define TOK_INTERFACE		276
#define TOK_RENEW		277
#define TOK_REBIND		278
#define TOK_EXPIRE		279
#define TOK_BOOTP		280
#define TOK_DENY		281
#define TOK_DEFAULT		282
#define TOK_MEDIA		283
#define TOK_MEDIUM		284
#define TOK_ALIAS		285
#define TOK_REBOOT		286
#define TOK_BACKOFF_CUTOFF	287
#define TOK_INITIAL_INTERVAL	288
#define TOK_SUPERSEDE		289
#define TOK_APPEND		290
#define TOK_PREPEND		291
#define TOK_REJECT		292
#define TOK_FDDI		293
#define TOK_LINK_TIMEOUT	294

#define is_identifier(x)	((x) >= TOK_FIRST_TOKEN &&	\
				 (x) != TOK_STRING &&	\
				 (x) != TOK_NUMBER &&	\
				 (x) != EOF)
