/*	$OpenBSD: mail.c,v 1.8 1999/01/10 17:55:03 millert Exp $	*/

/*
 * Mailbox checking code by Robert J. Gibson, adapted for PD ksh by
 * John R. MacMillan
 */

#include "config.h"

#ifdef KSH
#include "sh.h"
#include "ksh_stat.h"
#include "ksh_time.h"

#define MBMESSAGE	"you have mail in $_"

typedef struct mbox {
	struct mbox    *mb_next;	/* next mbox in list */
	char	       *mb_path;	/* path to mail file */
	char	       *mb_msg;		/* to announce arrival of new mail */
	time_t		mb_mtime;	/* mtime of mail file */
} mbox_t;

/*
 * $MAILPATH is a linked list of mboxes.  $MAIL is a treated as a
 * special case of $MAILPATH, where the list has only one node.  The
 * same list is used for both since they are exclusive.
 */

static mbox_t  *mplist;
static mbox_t  mbox;
static time_t	mlastchkd;	/* when mail was last checked */

static void     munset      ARGS((mbox_t *mlist)); /* free mlist and mval */
static mbox_t * mballoc     ARGS((char *p, char *m)); /* allocate a new mbox */
static void     mprintit    ARGS((mbox_t *mbp));

void
mcheck()
{
	register mbox_t	*mbp;
	time_t		 now;
	long		 mailcheck;
	struct tbl	*vp;
	struct stat	 stbuf;

	if (getint(global("MAILCHECK"), &mailcheck) < 0)
		return;

	now = time((time_t *) 0);
	if (mlastchkd == 0)
		mlastchkd = now;
	if (now - mlastchkd >= mailcheck) {
		mlastchkd = now;

		vp = global("MAILPATH");
		if (vp && (vp->flag & ISSET))
			mbp = mplist;
		else if ((vp = global("MAIL")) && (vp->flag & ISSET))
			mbp = &mbox;
		else
			mbp = NULL;

		while (mbp) {
			if (mbp->mb_path && stat(mbp->mb_path, &stbuf) == 0
			    && S_ISREG(stbuf.st_mode))
			{
				if (stbuf.st_size
				    && mbp->mb_mtime != stbuf.st_mtime
				    && stbuf.st_atime <= stbuf.st_mtime)
					mprintit(mbp);
				mbp->mb_mtime = stbuf.st_mtime;
			} else {
				/*
				 * Some mail readers remove the mail
				 * file if all mail is read.  If file
				 * does not exist, assume this is the
				 * case and set mtime to zero.
				 */
				mbp->mb_mtime = 0;
			}
			mbp = mbp->mb_next;
		}
	}
}

void
mbset(p)
	register char	*p;
{
	struct stat	stbuf;

	if (mbox.mb_msg)
		afree((void *)mbox.mb_msg, APERM);
	if (mbox.mb_path)
		afree((void *)mbox.mb_path, APERM);
	/* Save a copy to protect from export (which munges the string) */
	mbox.mb_path = str_save(p, APERM);
	mbox.mb_msg = NULL;
	if (p && stat(p, &stbuf) == 0 && S_ISREG(stbuf.st_mode))
		mbox.mb_mtime = stbuf.st_mtime;
	else
		mbox.mb_mtime = 0;
}

void
mpset(mptoparse)
	register char	*mptoparse;
{
	register mbox_t	*mbp;
	register char	*mpath, *mmsg, *mval;
	char *p;

	munset( mplist );
	mplist = NULL;
	mval = str_save(mptoparse, APERM);
	while (mval) {
		mpath = mval;
		if ((mval = strchr(mval, PATHSEP)) != NULL) {
			*mval = '\0', mval++;
		}
		/* POSIX/bourne-shell say file%message */
		for (p = mpath; (mmsg = strchr(p, '%')); ) {
			/* a literal percent? (POSIXism) */
			if (mmsg[-1] == '\\') {
				/* use memmove() to avoid overlap problems */
				memmove(mmsg - 1, mmsg, strlen(mmsg) + 1);
				p = mmsg + 1;
				continue;
			}
			break;
		}
		/* at&t ksh says file?message */
		if (!mmsg && !Flag(FPOSIX))
			mmsg = strchr(mpath, '?');
		if (mmsg) {
			*mmsg = '\0';
			mmsg++;
		}
		mbp = mballoc(mpath, mmsg);
		mbp->mb_next = mplist;
		mplist = mbp;
	}
}

static void
munset(mlist)
register mbox_t	*mlist;
{
	register mbox_t	*mbp;

	while (mlist != NULL) {
		mbp = mlist;
		mlist = mbp->mb_next;
		if (!mlist)
			afree((void *)mbp->mb_path, APERM);
		afree((void *)mbp, APERM);
	}
}

static mbox_t *
mballoc(p, m)
	char	*p;
	char	*m;
{
	struct stat	stbuf;
	register mbox_t	*mbp;

	mbp = (mbox_t *)alloc(sizeof(mbox_t), APERM);
	mbp->mb_next = NULL;
	mbp->mb_path = p;
	mbp->mb_msg = m;
	if (stat(mbp->mb_path, &stbuf) == 0 && S_ISREG(stbuf.st_mode))
		mbp->mb_mtime = stbuf.st_mtime;
	else
		mbp->mb_mtime = 0;
	return(mbp);
}

static void
mprintit( mbp )
mbox_t	*mbp;
{
	struct tbl	*vp;

#if 0
	/*
	 * I doubt this $_ overloading is bad in /bin/sh mode.  Anyhow, we
	 * crash as the code looks now if we do not set vp.  Now, this is
	 * easy to fix too, but I'd like to see what POSIX says before doing
	 * a change like that.
	 */
	if (!Flag(FSH))
#endif
		/* SETSTR: ignore fail (arbitrary; havn't checked at&t) */
		setstr((vp = local("_", FALSE)), mbp->mb_path);

	shellf("%s\n", substitute(mbp->mb_msg ? mbp->mb_msg : MBMESSAGE, 0));

	unset(vp, 0);
}
#endif /* KSH */
