/*	$OpenBSD: qop_conf.c,v 1.1.1.1 2001/06/27 18:23:30 kjc Exp $	*/
/*	$KAME: qop_conf.c,v 1.2 2000/10/18 09:15:19 kjc Exp $	*/
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

#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <stdio.h>

#include <altq/altq.h>
#include "altq_qop.h"

typedef int (interface_parser_t)(const char *, int, char **);
typedef int (class_parser_t)(const char *, const char *, const char *,
			     int, char **);

extern interface_parser_t	null_interface_parser;
extern class_parser_t		null_class_parser;
extern interface_parser_t	cbq_interface_parser;
extern class_parser_t		cbq_class_parser;
extern interface_parser_t	hfsc_interface_parser;
extern class_parser_t		hfsc_class_parser;
extern interface_parser_t	red_interface_parser;
extern interface_parser_t	rio_interface_parser;
extern interface_parser_t	blue_interface_parser;
extern interface_parser_t	wfq_interface_parser;
extern interface_parser_t	fifoq_interface_parser;
extern interface_parser_t	priq_interface_parser;
extern class_parser_t		priq_class_parser;

struct qdisc_parser qdisc_parser[] = {
	{"null",	null_interface_parser,	null_class_parser},
	{"cbq",		cbq_interface_parser,	cbq_class_parser},
	{"hfsc",	hfsc_interface_parser,	hfsc_class_parser},
	{"red",		red_interface_parser,	NULL},
	{"rio",		rio_interface_parser,	NULL},
	{"blue",	blue_interface_parser,	NULL},
	{"wfq",		wfq_interface_parser,	NULL},
	{"fifoq",	fifoq_interface_parser,	NULL},
	{"priq",	priq_interface_parser,	priq_class_parser},
	{NULL, NULL, NULL}
};

