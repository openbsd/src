/*	$NetBSD: svr4_ttold.c,v 1.6 1995/10/07 06:27:56 mycroft Exp $	 */

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
#include <compat/svr4/svr4_ttold.h>

static void svr4_tchars_to_bsd_tchars __P((const struct svr4_tchars *st,
					   struct tchars *bt));
static void bsd_tchars_to_svr4_tchars __P((const struct tchars *bt,
					   struct svr4_tchars *st));
static void svr4_sgttyb_to_bsd_sgttyb __P((const struct svr4_sgttyb *ss,
					   struct sgttyb *bs));
static void bsd_sgttyb_to_svr4_sgttyb __P((const struct sgttyb *bs,
					   struct svr4_sgttyb *ss));
static void svr4_ltchars_to_bsd_ltchars __P((const struct svr4_ltchars *sl,
					     struct ltchars *bl));
static void bsd_ltchars_to_svr4_ltchars __P((const struct ltchars *bl,
					     struct svr4_ltchars *sl));

static void
svr4_tchars_to_bsd_tchars(st, bt)
	const struct svr4_tchars	*st;
	struct tchars			*bt;
{
	bt->t_intrc  = st->t_intrc;
	bt->t_quitc  = st->t_quitc;
	bt->t_startc = st->t_startc;
	bt->t_stopc  = st->t_stopc;
	bt->t_eofc   = st->t_eofc;
	bt->t_brkc   = st->t_brkc;
}


static void
bsd_tchars_to_svr4_tchars(bt, st)
	const struct tchars	*bt;
	struct svr4_tchars	*st;
{
	st->t_intrc  = bt->t_intrc;
	st->t_quitc  = bt->t_quitc;
	st->t_startc = bt->t_startc;
	st->t_stopc  = bt->t_stopc;
	st->t_eofc   = bt->t_eofc;
	st->t_brkc   = bt->t_brkc;
}


static void
svr4_sgttyb_to_bsd_sgttyb(ss, bs)
	const struct svr4_sgttyb	*ss;
	struct sgttyb			*bs;
{
	bs->sg_ispeed = ss->sg_ispeed;
	bs->sg_ospeed = ss->sg_ospeed;
	bs->sg_erase  =	ss->sg_erase;	
	bs->sg_kill   = ss->sg_kill;
	bs->sg_flags  = ss->sg_flags;
};


static void
bsd_sgttyb_to_svr4_sgttyb(bs, ss)
	const struct sgttyb	*bs;
	struct svr4_sgttyb	*ss;
{
	ss->sg_ispeed = bs->sg_ispeed;
	ss->sg_ospeed = bs->sg_ospeed;
	ss->sg_erase  =	bs->sg_erase;	
	ss->sg_kill   = bs->sg_kill;
	ss->sg_flags  = bs->sg_flags;
}


static void
svr4_ltchars_to_bsd_ltchars(sl, bl)
	const struct svr4_ltchars	*sl;
	struct ltchars			*bl;
{
	bl->t_suspc  = sl->t_suspc;
	bl->t_dsuspc = sl->t_dsuspc;
	bl->t_rprntc = sl->t_rprntc;
	bl->t_flushc = sl->t_flushc;
	bl->t_werasc = sl->t_werasc;
	bl->t_lnextc = sl->t_lnextc;
}


static void
bsd_ltchars_to_svr4_ltchars(bl, sl)
	const struct ltchars	*bl;
	struct svr4_ltchars	*sl;
{
	sl->t_suspc  = bl->t_suspc;
	sl->t_dsuspc = bl->t_dsuspc;
	sl->t_rprntc = bl->t_rprntc;
	sl->t_flushc = bl->t_flushc;
	sl->t_werasc = bl->t_werasc;
	sl->t_lnextc = bl->t_lnextc;
}


int
svr4_ttoldioctl(fp, cmd, data, p, retval)
	struct file *fp;
	u_long cmd;
	caddr_t data;
	struct proc *p;
	register_t *retval;
{
	int			error;
	int (*ctl) __P((struct file *, u_long,  caddr_t, struct proc *)) =
			fp->f_ops->fo_ioctl;

	*retval = 0;

	switch (cmd) {
	case SVR4_TIOCGPGRP:
		{
			pid_t pid;

			if ((error = (*ctl)(fp, TIOCGPGRP,
					    (caddr_t) &pid, p)) != 0)
			    return error;

			DPRINTF(("TIOCGPGRP %d", pid));

			if ((error = copyout(&pid, data, sizeof(pid))) != 0)
				return error;

		}

	case SVR4_TIOCSPGRP:
		{
			pid_t pid;

			if ((error = copyin(data, &pid, sizeof(pid))) != 0)
				return error;

			DPRINTF(("TIOCSPGRP %d", pid));

			return (*ctl)(fp, TIOCSPGRP, (caddr_t) &pid, p);
		}

	case SVR4_TIOCGSID:
		{
			pid_t pid;

			if ((error = (*ctl)(fp, TIOCGSID,
					    (caddr_t) &pid, p)) != 0)
				return error;

			DPRINTF(("TIOCGSID %d", pid));

			return copyout(&pid, data, sizeof(pid));
		}

	case SVR4_TIOCGETP:
		{
			struct sgttyb bs;
			struct svr4_sgttyb ss;

			error = (*ctl)(fp, TIOCGETP, (caddr_t) &bs, p);
			if (error)
				return error;

			bsd_sgttyb_to_svr4_sgttyb(&bs, &ss);
			return copyout(&ss, data, sizeof(ss));
		}

	case SVR4_TIOCSETP:
	case SVR4_TIOCSETN:
		{
			struct sgttyb bs;
			struct svr4_sgttyb ss;

			if ((error = copyin(data, &ss, sizeof(ss))) != 0)
				return error;

			svr4_sgttyb_to_bsd_sgttyb(&ss, &bs);

			cmd = (cmd == SVR4_TIOCSETP) ? TIOCSETP : TIOCSETN;
			return (*ctl)(fp, cmd, (caddr_t) &bs, p);
		}

	case SVR4_TIOCGETC:
		{
			struct tchars bt;
			struct svr4_tchars st;

			error = (*ctl)(fp, TIOCGETC, (caddr_t) &bt, p);
			if (error)
				return error;

			bsd_tchars_to_svr4_tchars(&bt, &st);
			return copyout(&st, data, sizeof(st));
		}

	case SVR4_TIOCSETC:
		{
			struct tchars bt;
			struct svr4_tchars st;

			if ((error = copyin(data, &st, sizeof(st))) != 0)
				return error;

			svr4_tchars_to_bsd_tchars(&st, &bt);

			return (*ctl)(fp, TIOCSETC, (caddr_t) &bt, p);
		}

	case SVR4_TIOCGLTC:
		{
			struct ltchars bl;
			struct svr4_ltchars sl;

			error = (*ctl)(fp, TIOCGLTC, (caddr_t) &bl, p);
			if (error)
				return error;

			bsd_ltchars_to_svr4_ltchars(&bl, &sl);
			return copyout(&sl, data, sizeof(sl));
		}

	case SVR4_TIOCSLTC:
		{
			struct ltchars bl;
			struct svr4_ltchars sl;

			if ((error = copyin(data, &sl, sizeof(sl))) != 0)
				return error;

			svr4_ltchars_to_bsd_ltchars(&sl, &bl);

			return (*ctl)(fp, TIOCSLTC, (caddr_t) &bl, p);
		}

	default:
		DPRINTF(("Unknown svr4 ttold %x\n", cmd));
		return 0;	/* ENOSYS really */
	}
}
