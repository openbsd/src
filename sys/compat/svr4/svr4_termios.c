/*	$NetBSD: svr4_termios.c,v 1.5 1995/10/07 06:27:55 mycroft Exp $	 */

/*
 * Copyright (c) 1994 Christos Zoulas
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <sys/tty.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <net/if.h>
#include <sys/malloc.h>

#include <sys/syscallargs.h>

#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_util.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_syscallargs.h>
#include <compat/svr4/svr4_stropts.h>
#include <compat/svr4/svr4_termios.h>


#ifndef __CONCAT3
# if __STDC__
#  define __CONCAT3(a,b,c)	a ## b ## c
# else
#  define __CONCAT3(a,b,c)	a/**/b/**/c
# endif
#endif

static u_long bsd_to_svr4_speed __P((u_long sp, u_long mask));
static u_long svr4_to_bsd_speed __P((u_long sp, u_long mask));
static void svr4_to_bsd_termios __P((const struct svr4_termios *st, 
				     struct termios *bt));
static void bsd_to_svr4_termios __P((const struct termios *bt, 
				     struct svr4_termios *st));
static void svr4_termio_to_termios __P((const struct svr4_termio *t,
					struct svr4_termios *ts));
static void svr4_termios_to_termio __P((const struct svr4_termios *ts,
					struct svr4_termio *t));

#define undefined_char(a,b)				/**/
#define undefined_flag1(f,a,b)				/**/
#define undefined_flag2(f,a,b,c1,t1,c2,t2)		/**/
#define undefined_flag4(f,a,b,c1,t1,c2,t2,c3,t3,c4,t4)	/**/

#define svr4_to_bsd_char(a,b) \
	if (st->c_cc[__CONCAT(a,b)] == SVR4_POSIX_VDISABLE) \
		bt->c_cc[__CONCAT(a,b)] = _POSIX_VDISABLE; \
	else \
		bt->c_cc[__CONCAT(a,b)] = st->c_cc[__CONCAT3(SVR4_,a,b)]

#define svr4_to_bsd_flag1(f,a,b) \
	if (st->f & __CONCAT3(SVR4_,a,b)) \
		bt->f |= __CONCAT(a,b); \
	else \
		bt->f &= ~__CONCAT(a,b)

#define svr4_to_bsd_flag2(f,a,b,c1,t1,c2,t2) \
	bt->f &= ~__CONCAT(a,b); \
	switch (st->f & __CONCAT3(SVR4_,a,b)) { \
	case __CONCAT3(SVR4_,c1,t1): bt->f |= __CONCAT(c1,t1); break; \
	case __CONCAT3(SVR4_,c2,t2): bt->f |= __CONCAT(c2,t2); break; \
	}

#define svr4_to_bsd_flag4(f,a,b,c1,t1,c2,t2,c3,t3,c4,t4) \
	bt->f &= ~__CONCAT(a,b); \
	switch (st->f & __CONCAT3(SVR4_,a,b)) { \
	case __CONCAT3(SVR4_,c1,t1): bt->f |= __CONCAT(c1,t1); break; \
	case __CONCAT3(SVR4_,c2,t2): bt->f |= __CONCAT(c2,t2); break; \
	case __CONCAT3(SVR4_,c3,t3): bt->f |= __CONCAT(c3,t3); break; \
	case __CONCAT3(SVR4_,c4,t4): bt->f |= __CONCAT(c4,t4); break; \
	}


#define bsd_to_svr4_char(a,b) \
	if (bt->c_cc[__CONCAT(a,b)] == _POSIX_VDISABLE) \
		st->c_cc[__CONCAT3(SVR4_,a,b)] = SVR4_POSIX_VDISABLE; \
	else \
		st->c_cc[__CONCAT3(SVR4_,a,b)] = bt->c_cc[__CONCAT(a,b)]

#define bsd_to_svr4_flag1(f,a,b) \
	if (bt->f & __CONCAT(a,b)) \
		st->f |= __CONCAT3(SVR4_,a,b); \
	else \
		st->f &= ~__CONCAT3(SVR4_,a,b)

#define bsd_to_svr4_flag2(f,a,b,c1,t1,c2,t2) \
	st->f &= ~__CONCAT(a,b); \
	switch (bt->f & __CONCAT(a,b)) { \
	case __CONCAT(c1,t1): st->f |= __CONCAT3(SVR4_,c1,t1); break; \
	case __CONCAT(c2,t2): st->f |= __CONCAT3(SVR4_,c2,t2); break; \
	}

#define bsd_to_svr4_flag4(f,a,b,c1,t1,c2,t2,c3,t3,c4,t4) \
	st->f &= ~__CONCAT(a,b); \
	switch (bt->f & __CONCAT(a,b)) { \
	case __CONCAT(c1,t1): st->f |= __CONCAT3(SVR4_,c1,t1); break; \
	case __CONCAT(c2,t2): st->f |= __CONCAT3(SVR4_,c2,t2); break; \
	case __CONCAT(c3,t3): st->f |= __CONCAT3(SVR4_,c3,t3); break; \
	case __CONCAT(c4,t4): st->f |= __CONCAT3(SVR4_,c4,t4); break; \
	}

static u_long
bsd_to_svr4_speed(sp, mask)
	u_long sp;
	u_long mask;
{
	switch (sp) {
#undef getval
#define getval(a,b)	case __CONCAT(a,b):	sp = __CONCAT3(SVR4_,a,b)
	getval(B,0);
	getval(B,50);
	getval(B,75);
	getval(B,110);
	getval(B,134);
	getval(B,150);
	getval(B,200);
	getval(B,300);
	getval(B,600);
	getval(B,1200);
	getval(B,1800);
	getval(B,2400);
	getval(B,4800);
	getval(B,9600);
	getval(B,19200);
	getval(B,38400);
	default: sp = SVR4_B9600;	/* XXX */
	}

	while ((mask & 1) == 0) {
		mask >>= 1;
		sp <<= 1;
	}

	return sp;
}


static u_long
svr4_to_bsd_speed(sp, mask)
	u_long sp;
	u_long mask;
{
	while ((mask & 1) == 0) {
		mask >>= 1;
		sp >>= 1;
	}

	switch (sp & mask) {
#undef getval
#define getval(a,b)	case __CONCAT3(SVR4_,a,b):	return __CONCAT(a,b)
	getval(B,0);
	getval(B,50);
	getval(B,75);
	getval(B,110);
	getval(B,134);
	getval(B,150);
	getval(B,200);
	getval(B,300);
	getval(B,600);
	getval(B,1200);
	getval(B,1800);
	getval(B,2400);
	getval(B,4800);
	getval(B,9600);
	getval(B,19200);
	getval(B,38400);
	default: return B9600;	/* XXX */
	}
}


static void
svr4_to_bsd_termios(st, bt)
	const struct svr4_termios	*st;
	struct termios	 		*bt;
{
	/* control characters */
	svr4_to_bsd_char(V,INTR);
	svr4_to_bsd_char(V,QUIT);
	svr4_to_bsd_char(V,ERASE);
	svr4_to_bsd_char(V,KILL);
	svr4_to_bsd_char(V,EOF);
	svr4_to_bsd_char(V,EOL);
	svr4_to_bsd_char(V,EOL2);
	svr4_to_bsd_char(V,MIN);
	svr4_to_bsd_char(V,TIME);
	undefined_char(V,SWTCH);
	svr4_to_bsd_char(V,START);
	svr4_to_bsd_char(V,STOP);
	svr4_to_bsd_char(V,SUSP);
	svr4_to_bsd_char(V,DSUSP);
	svr4_to_bsd_char(V,REPRINT);
	svr4_to_bsd_char(V,DISCARD);
	svr4_to_bsd_char(V,WERASE);
	svr4_to_bsd_char(V,LNEXT);

	/* Input modes */
	svr4_to_bsd_flag1(c_iflag,I,GNBRK);
	svr4_to_bsd_flag1(c_iflag,B,RKINT);
	svr4_to_bsd_flag1(c_iflag,I,GNPAR);
	svr4_to_bsd_flag1(c_iflag,P,ARMRK);
	svr4_to_bsd_flag1(c_iflag,I,NPCK);
	svr4_to_bsd_flag1(c_iflag,I,STRIP);
	svr4_to_bsd_flag1(c_iflag,I,NLCR);
	svr4_to_bsd_flag1(c_iflag,I,GNCR);
	svr4_to_bsd_flag1(c_iflag,I,CRNL);
	undefined_flag1(c_iflag,I,UCLC);
	svr4_to_bsd_flag1(c_iflag,I,XON);
	svr4_to_bsd_flag1(c_iflag,I,XANY);
	svr4_to_bsd_flag1(c_iflag,I,XOFF);
	svr4_to_bsd_flag1(c_iflag,I,MAXBEL);
	undefined_flag1(c_iflag,D,OSMODE);

	/* Output modes */
	svr4_to_bsd_flag1(c_oflag,O,POST);
	undefined_flag1(c_oflag,O,LCUC);
	svr4_to_bsd_flag1(c_oflag,O,NLCR);
	undefined_flag1(c_oflag,O,CRNL);
	undefined_flag1(c_oflag,O,NOCR);
	undefined_flag1(c_oflag,O,NLRET);
	undefined_flag1(c_oflag,O,FILL);
	undefined_flag1(c_oflag,O,FDEL);
	undefined_flag2(c_oflag,N,LDLY,N,L0,N,L1);
	undefined_flag4(c_oflag,C,RDLY,C,R0,C,R1,C,R2,C,R3);
	undefined_flag4(c_oflag,T,ABDLY,T,AB0,T,AB1,T,AB2,T,AB3);
	undefined_flag2(c_oflag,B,SDLY,B,S0,B,S1);
	undefined_flag2(c_oflag,V,TDLY,V,T0,V,T1);
	undefined_flag2(c_oflag,F,FDLY,F,F0,F,F1);
	undefined_flag1(c_oflag,P,AGEOUT);
	undefined_flag1(c_oflag,W,RAP);

	/* Control modes */
	bt->c_ospeed = svr4_to_bsd_speed(st->c_cflag, SVR4_CBAUD);
	svr4_to_bsd_flag4(c_cflag,C,SIZE,C,S5,C,S6,C,S7,C,S8)
	svr4_to_bsd_flag1(c_cflag,C,STOPB);
	svr4_to_bsd_flag1(c_cflag,C,READ);
	svr4_to_bsd_flag1(c_cflag,P,ARENB);
	svr4_to_bsd_flag1(c_cflag,P,ARODD);
	svr4_to_bsd_flag1(c_cflag,H,UPCL);
	svr4_to_bsd_flag1(c_cflag,C,LOCAL);
	undefined_flag1(c_cflag,R,CV1EN);
	undefined_flag1(c_cflag,X,MT1EN);
	undefined_flag1(c_cflag,L,OBLK);
	undefined_flag1(c_cflag,X,CLUDE);
	bt->c_ispeed = svr4_to_bsd_speed(st->c_cflag, SVR4_CIBAUD);
	undefined_flag1(c_cflag,P,AREXT);

	/* line discipline modes */
	svr4_to_bsd_flag1(c_lflag,I,SIG);
	svr4_to_bsd_flag1(c_lflag,I,CANON);
	undefined_flag1(c_lflag,X,CASE);
	svr4_to_bsd_flag1(c_lflag,E,CHO);
	svr4_to_bsd_flag1(c_lflag,E,CHOE);
	svr4_to_bsd_flag1(c_lflag,E,CHOK);
	svr4_to_bsd_flag1(c_lflag,E,CHONL);
	svr4_to_bsd_flag1(c_lflag,N,OFLSH);
	svr4_to_bsd_flag1(c_lflag,T,OSTOP);
	svr4_to_bsd_flag1(c_lflag,E,CHOCTL);
	svr4_to_bsd_flag1(c_lflag,E,CHOPRT);
	svr4_to_bsd_flag1(c_lflag,E,CHOKE);
	undefined_flag1(c_lflag,D,EFECHO);
	svr4_to_bsd_flag1(c_lflag,F,LUSHO);
	svr4_to_bsd_flag1(c_lflag,P,ENDIN);
	svr4_to_bsd_flag1(c_lflag,I,EXTEN);
}


static void
bsd_to_svr4_termios(bt, st)
	const struct termios 	*bt;
	struct svr4_termios	*st;
{
	/* control characters */
	bsd_to_svr4_char(V,INTR);
	bsd_to_svr4_char(V,QUIT);
	bsd_to_svr4_char(V,ERASE);
	bsd_to_svr4_char(V,KILL);
	bsd_to_svr4_char(V,EOF);
	bsd_to_svr4_char(V,EOL);
	bsd_to_svr4_char(V,EOL2);
	bsd_to_svr4_char(V,MIN);
	bsd_to_svr4_char(V,TIME);
	undefined_char(V,SWTCH);
	bsd_to_svr4_char(V,START);
	bsd_to_svr4_char(V,STOP);
	bsd_to_svr4_char(V,SUSP);
	bsd_to_svr4_char(V,DSUSP);
	bsd_to_svr4_char(V,REPRINT);
	bsd_to_svr4_char(V,DISCARD);
	bsd_to_svr4_char(V,WERASE);
	bsd_to_svr4_char(V,LNEXT);

	/* Input modes */
	bsd_to_svr4_flag1(c_iflag,I,GNBRK);
	bsd_to_svr4_flag1(c_iflag,B,RKINT);
	bsd_to_svr4_flag1(c_iflag,I,GNPAR);
	bsd_to_svr4_flag1(c_iflag,P,ARMRK);
	bsd_to_svr4_flag1(c_iflag,I,NPCK);
	bsd_to_svr4_flag1(c_iflag,I,STRIP);
	bsd_to_svr4_flag1(c_iflag,I,NLCR);
	bsd_to_svr4_flag1(c_iflag,I,GNCR);
	bsd_to_svr4_flag1(c_iflag,I,CRNL);
	undefined_flag1(c_iflag,I,UCLC);
	bsd_to_svr4_flag1(c_iflag,I,XON);
	bsd_to_svr4_flag1(c_iflag,I,XANY);
	bsd_to_svr4_flag1(c_iflag,I,XOFF);
	bsd_to_svr4_flag1(c_iflag,I,MAXBEL);
	undefined_flag1(c_iflag,D,OSMODE);

	/* Output modes */
	bsd_to_svr4_flag1(c_oflag,O,POST);
	undefined_flag1(c_oflag,O,LCUC);
	undefined_flag1(c_oflag,O,NLCR);
	undefined_flag1(c_oflag,O,CRNL);
	undefined_flag1(c_oflag,O,NOCR);
	undefined_flag1(c_oflag,O,NLRET);
	undefined_flag1(c_oflag,O,FILL);
	undefined_flag1(c_oflag,O,FDEL);
	undefined_flag2(c_oflag,N,LDLY,N,L0,N,L1);
	undefined_flag4(c_oflag,C,RDLY,C,R0,C,R1,C,R2,C,R3);
	undefined_flag4(c_oflag,T,ABDLY,T,AB0,T,AB1,T,AB2,T,AB3);
	undefined_flag2(c_oflag,B,SDLY,B,S0,B,S1);
	undefined_flag2(c_oflag,V,TDLY,V,T0,V,T1);
	undefined_flag2(c_oflag,F,FDLY,F,F0,F,F1);
	undefined_flag1(c_oflag,P,AGEOUT);
	undefined_flag1(c_oflag,W,RAP);

	/* Control modes */
	st->c_cflag &= ~SVR4_CBAUD;
	st->c_cflag |= bsd_to_svr4_speed(bt->c_ospeed, SVR4_CBAUD);
	bsd_to_svr4_flag4(c_cflag,C,SIZE,C,S5,C,S6,C,S7,C,S8)
	bsd_to_svr4_flag1(c_cflag,C,STOPB);
	bsd_to_svr4_flag1(c_cflag,C,READ);
	bsd_to_svr4_flag1(c_cflag,P,ARENB);
	bsd_to_svr4_flag1(c_cflag,P,ARODD);
	bsd_to_svr4_flag1(c_cflag,H,UPCL);
	bsd_to_svr4_flag1(c_cflag,C,LOCAL);
	undefined_flag1(c_cflag,R,CV1EN);
	undefined_flag1(c_cflag,X,MT1EN);
	undefined_flag1(c_cflag,L,OBLK);
	undefined_flag1(c_cflag,X,CLUDE);
	st->c_cflag &= ~SVR4_CIBAUD;
	st->c_cflag |= bsd_to_svr4_speed(bt->c_ispeed, SVR4_CIBAUD);

	undefined_flag1(c_oflag,P,AREXT);

	/* line discipline modes */
	bsd_to_svr4_flag1(c_lflag,I,SIG);
	bsd_to_svr4_flag1(c_lflag,I,CANON);
	undefined_flag1(c_lflag,X,CASE);
	bsd_to_svr4_flag1(c_lflag,E,CHO);
	bsd_to_svr4_flag1(c_lflag,E,CHOE);
	bsd_to_svr4_flag1(c_lflag,E,CHOK);
	bsd_to_svr4_flag1(c_lflag,E,CHONL);
	bsd_to_svr4_flag1(c_lflag,N,OFLSH);
	bsd_to_svr4_flag1(c_lflag,T,OSTOP);
	bsd_to_svr4_flag1(c_lflag,E,CHOCTL);
	bsd_to_svr4_flag1(c_lflag,E,CHOPRT);
	bsd_to_svr4_flag1(c_lflag,E,CHOKE);
	undefined_flag1(c_lflag,D,EFECHO);
	bsd_to_svr4_flag1(c_lflag,F,LUSHO);
	bsd_to_svr4_flag1(c_lflag,P,ENDIN);
	bsd_to_svr4_flag1(c_lflag,I,EXTEN);
}


static void
svr4_termio_to_termios(t, ts)
	const struct svr4_termio	*t;
	struct svr4_termios		*ts;
{
	int i;

	ts->c_iflag = (svr4_tcflag_t) t->c_iflag;
	ts->c_oflag = (svr4_tcflag_t) t->c_oflag;
	ts->c_cflag = (svr4_tcflag_t) t->c_cflag;
	ts->c_lflag = (svr4_tcflag_t) t->c_lflag;

	for (i = 0; i < SVR4_NCC; i++)
		ts->c_cc[i] = (svr4_cc_t) t->c_cc[i];
}


static void
svr4_termios_to_termio(ts, t)
	const struct svr4_termios	*ts;
	struct svr4_termio		*t;
{
	int i;

	t->c_iflag = (u_short) ts->c_iflag;
	t->c_oflag = (u_short) ts->c_oflag;
	t->c_cflag = (u_short) ts->c_cflag;
	t->c_lflag = (u_short) ts->c_lflag;
	t->c_line = 0;	/* XXX */

	for (i = 0; i < SVR4_NCC; i++)
		t->c_cc[i] = (u_char) ts->c_cc[i];
}

int
svr4_termioctl(fp, cmd, data, p, retval)
	struct file *fp;
	u_long cmd;
	caddr_t data;
	struct proc *p;
	register_t *retval;
{
	struct termios 		bt;
	struct svr4_termios	st;
	struct svr4_termio	t;
	int			error;
	int (*ctl) __P((struct file *, u_long,  caddr_t, struct proc *)) =
			fp->f_ops->fo_ioctl;

	*retval = 0;

	switch (cmd) {
	case SVR4_TCGETA:
	case SVR4_TCGETS:
		if ((error = (*ctl)(fp, TIOCGETA, (caddr_t) &bt, p)) != 0)
			return error;

#ifdef DEBUG_SVR4
		{
			int i;
			printf("iflag=%o oflag=%o cflag=%o lflag=%o\n",
			       bt.c_iflag, bt.c_oflag, bt.c_lflag);
			printf("cc: ");
			for (i = 0; i < NCCS; i++)
				printf("%o ", bt.c_cc[i]);
			printf("\n");
		}
#endif

		bsd_to_svr4_termios(&bt, &st);

		DPRINTF(("ioctl(TCGET[A|S]);\n"));

#ifdef DEBUG_SVR4
		{
			int i;
			printf("iflag=%o oflag=%o cflag=%o lflag=%o\n",
			       st.c_iflag, st.c_oflag, st.c_lflag);
			printf("cc: ");
			for (i = 0; i < SVR4_NCCS; i++)
				printf("%o ", st.c_cc[i]);
			printf("\n");
		}
#endif

		if (cmd == SVR4_TCGETA) {
		    svr4_termios_to_termio(&st, &t);
		    return copyout(&t, data, sizeof(t));
		}
		else  {
		    return copyout(&st, data, sizeof(st));
		}

	case SVR4_TCSETA:
	case SVR4_TCSETS:
	case SVR4_TCSETAW:
	case SVR4_TCSETSW:
	case SVR4_TCSETAF:
	case SVR4_TCSETSF:
		/* get full BSD termios so we don't lose information */
		if ((error = (*ctl)(fp, TIOCGETA, (caddr_t) &bt, p)) != 0)
			return error;

		switch (cmd) {
		case SVR4_TCSETS:
		case SVR4_TCSETSW:
		case SVR4_TCSETSF:
			if ((error = copyin(data, &st, sizeof(st))) != 0)
				return error;
			break;

		case SVR4_TCSETA:
		case SVR4_TCSETAW:
		case SVR4_TCSETAF:
			if ((error = copyin(data, &t, sizeof(t))) != 0)
				return error;

			svr4_termio_to_termios(&t, &st);
			break;
		}

		svr4_to_bsd_termios(&st, &bt);

		switch (cmd) {
		case SVR4_TCSETA:
		case SVR4_TCSETS:
			DPRINTF(("ioctl(TCSET[A|S]);\n"));
			cmd = TIOCSETA;
			break;
		case SVR4_TCSETAW:
		case SVR4_TCSETSW:
			DPRINTF(("ioctl(TCSET[A|S]W);\n"));
			cmd = TIOCSETAW;
			break;
		case SVR4_TCSETAF:
		case SVR4_TCSETSF:
			DPRINTF(("ioctl(TCSET[A|S]F);\n"));
			cmd = TIOCSETAF;
			break;
		}

		return (*ctl)(fp, cmd, (caddr_t) &bt, p);

	default:
		DPRINTF(("Unknown svr4 termios %x\n", cmd));
		return ENOSYS;
	}
}
