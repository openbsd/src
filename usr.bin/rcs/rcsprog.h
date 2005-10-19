/*	$OpenBSD: rcsprog.h,v 1.16 2005/10/19 00:30:22 joris Exp $	*/
/*
 * Copyright (c) 2005 Joris Vink <joris@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RCSPROG_H
#define RCSPROG_H

extern char *__progname;
extern const char rcs_version[];
extern int verbose;
extern int pipeout;

extern int rcs_optind;
extern char *rcs_optarg;

/* date.y */
time_t  cvs_date_parse(const char *);

void	rcs_usage(void);
void	checkout_usage(void);
void	checkin_usage(void);
void	rcsdiff_usage(void);
void	rcsclean_usage(void);
void	rcsmerge_usage(void);
void	rlog_usage(void);
void	ident_usage(void);
void	(*usage)(void);
void	rcs_set_rev(const char *, RCSNUM **);

int	rcs_init(char *, char **, int);
int	rcs_getopt(int, char **, const char *);
int	rcs_statfile(char *, char *, size_t);
int	checkout_rev(RCSFILE *, RCSNUM *, const char *, int, const char *, int);
int	checkout_main(int, char **);
int	checkin_main(int, char **);
int	rcs_main(int, char **);
int	rcsdiff_main(int, char **);
int	rcsclean_main(int, char **);
int	rcsmerge_main(int, char **);
int	rlog_main(int, char **);
int	ident_main(int, char **);

#endif	/* RCSPROG_H */
