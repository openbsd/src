/*	$OpenBSD: hist.c,v 1.13 2006/01/02 08:11:56 xsa Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
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

#include "includes.h"

#include "cvs.h"
#include "log.h"

#define CVS_HIST_BUFSIZE	8192



static int	cvs_hist_fillbuf(CVSHIST *);
static int	cvs_hist_fmt(const struct cvs_hent *, char *, size_t);


/*
 * cvs_hist_open()
 *
 * Open a CVS history file.
 * Returns the number of entries in the file on success, or -1 on error.
 */
CVSHIST *
cvs_hist_open(const char *path)
{
	CVSHIST *histp;

	histp = (CVSHIST *)xmalloc(sizeof(*histp));
	memset(histp, 0, sizeof(*histp));

	histp->chf_buf = (char *)xmalloc((size_t)CVS_HIST_BUFSIZE);
	histp->chf_blen = CVS_HIST_BUFSIZE;
	histp->chf_off = 0;

	histp->chf_sindex = 0;
	histp->chf_cindex = 0;
	histp->chf_nbhent = 0;

	cvs_log(LP_TRACE, "cvs_hist_open(%s)", path);

	histp->chf_fd = open(path, O_RDONLY, 0);
	if (histp->chf_fd == -1) {
		cvs_log(LP_ERRNO,
		    "failed to open CVS history file `%s'", path);
		cvs_nolog = 1;
		xfree(histp->chf_buf);
		xfree(histp);
		return (NULL);
	}

	cvs_hist_fillbuf(histp);

	return (histp);
}


/*
 * cvs_hist_close()
 *
 * Close the CVS history file previously opened by a call to cvs_hist_open()
 */
void
cvs_hist_close(CVSHIST *histp)
{
	if (histp->chf_fd >= 0)
		(void)close(histp->chf_fd);
	xfree(histp->chf_buf);
	xfree(histp);
}


/*
 * cvs_hist_getnext()
 *
 * Get the next entry from the history file <histp>.  Whenever using this
 * function, it should be assumed that the return value of the previous call
 * to cvs_hist_getnext() is now invalid.
 * Returns the next entry from the file on success, or NULL on failure or if
 * no entries are left.
 */
struct cvs_hent *
cvs_hist_getnext(CVSHIST *histp)
{
	if (histp->chf_cindex == histp->chf_nbhent) {
		/* no more cached entries, refill buf and parse */
		cvs_hist_fillbuf(histp);
		cvs_hist_parse(histp);
	}
	return (&(histp->chf_hent[histp->chf_cindex++]));
}


/*
 * cvs_hist_append()
 *
 * Append a history entry to the history file <histp>.  The file offset is
 * first set to the end of the file.
 * Returns 0 on success, or -1 on failure.
 */
int
cvs_hist_append(CVSHIST *histp, struct cvs_hent *hentp)
{
	char hbuf[128];

	if (cvs_nolog == 1)
		return (0);

	if (cvs_hist_fmt(hentp, hbuf, sizeof(hbuf)) < 0) {
		cvs_log(LP_ERR, "failed to append CVS history entry");
		return (-1);
	}

	/* position ourself at the end */
	if (lseek(histp->chf_fd, (off_t)0, SEEK_END) == -1) {
		cvs_log(LP_ERRNO, "failed to seek to end of CVS history file");
		return (-1);
	}

	if (write(histp->chf_fd, hbuf, strlen(hbuf)) == -1) {
		cvs_log(LP_ERR, "failed to write CVS history entry to file");
		return (-1);
	}

	return (0);
}



/*
 * cvs_hist_fillbuf()
 *
 * Fill the history file's internal buffer for future parsing.
 */
static int
cvs_hist_fillbuf(CVSHIST *histp)
{
	ssize_t ret;

	/* reposition ourself in case we're missing the start of a record */
	if (lseek(histp->chf_fd, histp->chf_off, SEEK_SET) == -1) {
		cvs_log(LP_ERRNO, "failed to seek in CVS history file");
		return (-1);
	}
	ret = read(histp->chf_fd, histp->chf_buf, histp->chf_blen);
	if (ret == -1) {
		cvs_log(LP_ERRNO, "failed to buffer CVS history file");
		return (-1);
	} else {
		histp->chf_bused = (size_t)ret;
	}

	return (ret);
}


/*
 * cvs_hist_parse()
 *
 * Parse the current contents of the internal buffer of <histp> and regenerate
 * the buffered history entries.
 * Returns the number of entries parsed on success, or -1 on failure.
 */
int
cvs_hist_parse(CVSHIST *histp)
{
	u_int i, fld;
	char *fields[CVS_HIST_NBFLD], *sp, *bep, *ep, *errp;

	sp = histp->chf_buf;
	bep = histp->chf_buf + histp->chf_bused - 1;

	for (i = 0; i < CVS_HIST_CACHE; i++) {
		ep = memchr(sp, '\n', bep - sp);
		if (ep == NULL) {
			/*
			 * No record or incomplete record left to parse,
			 * so adjust the next read offset in consequence.
			 */
			histp->chf_off += (off_t)(sp - histp->chf_buf);
			break;
		} else if (ep == bep) {
			histp->chf_off += (off_t)histp->chf_bused;
		}
		*(ep++) = '\0';

		printf("hist(%s)\n", sp);

		histp->chf_hent[i].ch_event = *sp++;

		/* split the record in fields */
		fields[0] = sp;

		fld = 1;
		while (sp < ep) {
			if (*sp == '|') {
				*sp = '\0';
				fields[fld++] = sp + 1;
			}
			if (fld == CVS_HIST_NBFLD)
				break;
			sp++;
		}
#if 0
		for (fld = 0; fld < CVS_HIST_NBFLD; fld++)
			printf("fields[%u] = `%s'\n", fld, fields[fld]);
#endif

		histp->chf_hent[i].ch_date = (time_t)strtol(fields[0],
		    &errp, 16);
		if (*errp != '\0') {
			cvs_log(LP_ERR,
			    "parse error in date field of CVS history entry");
			continue;
		}

		histp->chf_hent[i].ch_user = fields[1];
		histp->chf_hent[i].ch_curdir = fields[2];
		histp->chf_hent[i].ch_repo = fields[3];
		histp->chf_hent[i].ch_rev = rcsnum_alloc();
		rcsnum_aton(fields[4], NULL, histp->chf_hent[i].ch_rev);
		histp->chf_hent[i].ch_arg = fields[5];
		sp = ep;
	}

	/* update indexes */
	histp->chf_sindex += histp->chf_nbhent;
	histp->chf_nbhent = i;
	histp->chf_cindex = 0;


	return (i);
}


/*
 * cvs_hist_fmt()
 *
 * Format the contents of the CVS history entry <ent> into the format used in
 * the CVS `history' file, and store the resulting string in <buf>, which is
 * of size <blen>.
 */
static int
cvs_hist_fmt(const struct cvs_hent *ent, char *buf, size_t blen)
{
	char numbuf[64];
	int len;

	if (rcsnum_tostr(ent->ch_rev, numbuf, sizeof(numbuf)) == NULL)
		return (-1);

	len = snprintf(buf, blen, "%c%8x|%s|%s|%s|%s|%s",
	    ent->ch_event, ent->ch_date, ent->ch_user, ent->ch_curdir,
	    ent->ch_repo, numbuf, ent->ch_arg);
	if (len >= (int)blen || len == -1)
		return (-1);
	return (len);
}
