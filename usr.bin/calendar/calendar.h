/*	$OpenBSD: calendar.h,v 1.4 1998/12/13 07:31:07 pjanzen Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 */


extern struct passwd *pw;
extern int doall;
extern time_t f_time;
extern struct iovec header[];
extern struct tm *tp;
extern char *calendarFile;
extern char *optarg;

struct fixs {
	char *name;
	int len;
};

struct event {
	time_t	when;
	char	print_date[31];
	char	**desc;
	char	*ldesc;
	struct event	*next;
};

struct match {
	time_t	when;
	char	print_date[30];
	int	var;
	struct match	*next;
};

struct specialev {
	char *name;
	int nlen;
	char *uname;
	int ulen;
	int (*getev) __P((int));
};

void	 cal __P((void));
void	 closecal __P((FILE *));
int	 getday __P((char *));
int	 getdayvar __P((char *));
int	 getfield __P((char *, char **, int *));
int	 getmonth __P((char *));
int	 easter __P((int));
int	 paskha __P((int));
void	 insert __P((struct event **, struct event *));
struct match	*isnow __P((char *));
FILE	*opencal __P((void));
void	 settime __P((time_t *));
time_t	 Mktime __P((char *));
void	 usage __P((void));
int	 foy __P((int));
void	 variable_weekday __P((int *, int, int));
void	 setnnames __P((void));

/* some flags */
#define	F_ISMONTH	0x01 /* month (Januar ...) */
#define	F_ISDAY		0x02 /* day of week (Sun, Mon, ...) */
/*#define	F_ISDAYVAR	0x04  variables day of week, like SundayLast */
#define	F_SPECIAL	0x08 /* Events that occur once a year but don't track
			      * calendar time--e.g.  Easter or easter depending
			      * days */

extern int f_dayAfter;	/* days after current date */
extern int f_dayBefore;	/* days before current date */

/* Special events; see also setnnames() in day.c */
/* '=' is not a valid character in a special event name */
#define EASTER "easter"
#define EASTERNAMELEN (sizeof(EASTER) - 1)
#define PASKHA "paskha"
#define PASKHALEN (sizeof(PASKHA) - 1)

#define NUMEV 2	/* Total number of such special events */
extern struct specialev spev[NUMEV];
