/*	$OpenBSD: diff.c,v 1.56 2005/10/05 23:11:06 niallo Exp $	*/
/*
 * Copyright (C) Caldera International Inc.  2001-2002.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code and documentation must retain the above
 *    copyright notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed or owned by Caldera
 *	International, Inc.
 * 4. Neither the name of Caldera International, Inc. nor the names of other
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE FOR ANY DIRECT,
 * INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2004 Jean-Francois Brousseau.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)diffreg.c   8.1 (Berkeley) 6/6/93
 */
/*
 *	Uses an algorithm due to Harold Stone, which finds
 *	a pair of longest identical subsequences in the two
 *	files.
 *
 *	The major goal is to generate the match vector J.
 *	J[i] is the index of the line in file1 corresponding
 *	to line i file0. J[i] = 0 if there is no
 *	such line in file1.
 *
 *	Lines are hashed so as to work in core. All potential
 *	matches are located by sorting the lines of each file
 *	on the hash (called ``value''). In particular, this
 *	collects the equivalence classes in file1 together.
 *	Subroutine equiv replaces the value of each line in
 *	file0 by the index of the first element of its
 *	matching equivalence in (the reordered) file1.
 *	To save space equiv squeezes file1 into a single
 *	array member in which the equivalence classes
 *	are simply concatenated, except that their first
 *	members are flagged by changing sign.
 *
 *	Next the indices that point into member are unsorted into
 *	array class according to the original order of file0.
 *
 *	The cleverness lies in routine stone. This marches
 *	through the lines of file0, developing a vector klist
 *	of "k-candidates". At step i a k-candidate is a matched
 *	pair of lines x,y (x in file0 y in file1) such that
 *	there is a common subsequence of length k
 *	between the first i lines of file0 and the first y
 *	lines of file1, but there is no such subsequence for
 *	any smaller y. x is the earliest possible mate to y
 *	that occurs in such a subsequence.
 *
 *	Whenever any of the members of the equivalence class of
 *	lines in file1 matable to a line in file0 has serial number
 *	less than the y of some k-candidate, that k-candidate
 *	with the smallest such y is replaced. The new
 *	k-candidate is chained (via pred) to the current
 *	k-1 candidate so that the actual subsequence can
 *	be recovered. When a member has serial number greater
 *	that the y of all k-candidates, the klist is extended.
 *	At the end, the longest subsequence is pulled out
 *	and placed in the array J by unravel
 *
 *	With J in hand, the matches there recorded are
 *	check'ed against reality to assure that no spurious
 *	matches have crept in due to hashing. If they have,
 *	they are broken, and "jackpot" is recorded--a harmless
 *	matter except that a true match for a spuriously
 *	mated line may now be unnecessarily reported as a change.
 *
 *	Much of the complexity of the program comes simply
 *	from trying to minimize core utilization and
 *	maximize the range of doable problems by dynamically
 *	allocating what is needed and reusing what is not.
 *	The core requirements for problems larger than somewhat
 *	are (in words) 2*length(file0) + length(file1) +
 *	3*(number of k-candidates installed),  typically about
 *	6n words for files of length n.
 */

#include <sys/stat.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <regex.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "buf.h"
#include "cvs.h"
#include "diff.h"
#include "log.h"
#include "proto.h"

struct cand {
	int	x;
	int	y;
	int	pred;
} cand;

struct line {
	int	serial;
	int	value;
} *file[2];

/*
 * The following struct is used to record change information when
 * doing a "context" or "unified" diff.  (see routine "change" to
 * understand the highly mnemonic field names)
 */
struct context_vec {
	int	a;	/* start line in old file */
	int	b;	/* end line in old file */
	int	c;	/* start line in new file */
	int	d;	/* end line in new file */
};

struct diff_arg {
	char	*rev1;
	char	*rev2;
	char	*date1;
	char	*date2;
};

#if !defined(RCSPROG)
static int     cvs_diff_init(struct cvs_cmd *, int, char **, int *);
static int	cvs_diff_remote(CVSFILE *, void *);
static int	cvs_diff_local(CVSFILE *, void *);
static int	cvs_diff_pre_exec(struct cvsroot *);
static int	cvs_diff_cleanup(void);
#endif

static void	 output(const char *, FILE *, const char *, FILE *);
static void	 check(FILE *, FILE *);
static void	 range(int, int, char *);
static void	 uni_range(int, int);
static void	 dump_context_vec(FILE *, FILE *);
static void	 dump_unified_vec(FILE *, FILE *);
static int	 prepare(int, FILE *, off_t);
static void	 prune(void);
static void	 equiv(struct line *, int, struct line *, int, int *);
static void	 unravel(int);
static void	 unsort(struct line *, int, int *);
static void	 change(const char *, FILE *, const char *, FILE *, int,
		    int, int, int);
static void	 sort(struct line *, int);
static int	 ignoreline(char *);
static int	 asciifile(FILE *);
static int	 fetch(long *, int, int, FILE *, int, int);
static int	 newcand(int, int, int);
static int	 search(int *, int, int);
static int	 skipline(FILE *);
static int	 isqrt(int);
static int	 stone(int *, int, int *, int *);
static int	 readhash(FILE *);
static int	 files_differ(FILE *, FILE *);
static char	*match_function(const long *, int, FILE *);
static char	*preadline(int, size_t, off_t);


#if !defined(RCSPROG)
static int Nflag;
static char diffargs[128];
#endif
static int aflag, bflag, dflag, iflag, pflag, tflag, Tflag, wflag;
static int context;
static int format = D_NORMAL;
static struct stat stb1, stb2;
static char *ifdefname, *ignore_pats;
static const char *diff_file;
regex_t ignore_re;

static int  *J;			/* will be overlaid on class */
static int  *class;		/* will be overlaid on file[0] */
static int  *klist;		/* will be overlaid on file[0] after class */
static int  *member;		/* will be overlaid on file[1] */
static int   clen;
static int   inifdef;		/* whether or not we are in a #ifdef block */
static int   diff_len[2];
static int   pref, suff;	/* length of prefix and suffix */
static int   slen[2];
static int   anychange;
static long *ixnew;		/* will be overlaid on file[1] */
static long *ixold;		/* will be overlaid on klist */
static struct cand *clist;	/* merely a free storage pot for candidates */
static int   clistlen;		/* the length of clist */
static struct line *sfile[2];	/* shortened by pruning common prefix/suffix */
static u_char *chrtran;		/* translation table for case-folding */
static struct context_vec *context_vec_start;
static struct context_vec *context_vec_end;
static struct context_vec *context_vec_ptr;

#define FUNCTION_CONTEXT_SIZE	41
static char lastbuf[FUNCTION_CONTEXT_SIZE];
static int  lastline;
static int  lastmatchline;
/*
 * chrtran points to one of 2 translation tables: cup2low if folding upper to
 * lower case clow2low if not folding case
 */
u_char clow2low[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
	0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
	0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
	0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
	0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41,
	0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c,
	0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62,
	0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d,
	0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
	0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83,
	0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e,
	0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
	0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4,
	0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba,
	0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5,
	0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
	0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb,
	0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6,
	0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1,
	0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc,
	0xfd, 0xfe, 0xff
};

u_char cup2low[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
	0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
	0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
	0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
	0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x60, 0x61,
	0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c,
	0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x60, 0x61, 0x62,
	0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d,
	0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
	0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83,
	0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e,
	0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
	0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4,
	0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba,
	0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5,
	0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
	0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb,
	0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6,
	0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1,
	0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc,
	0xfd, 0xfe, 0xff
};

#if !defined(RCSPROG)
struct cvs_cmd cvs_cmd_diff = {
	CVS_OP_DIFF, CVS_REQ_DIFF, "diff",
	{ "di", "dif" },
	"Show differences between revisions",
	"[-cilNnpu] [[-D date] [-r rev] [-D date2 | -r rev2]] "
	"[-k mode] [file ...]",
	"cD:iklNnpr:Ru",
	NULL,
	CF_RECURSE | CF_IGNORE | CF_SORT | CF_KNOWN,
	cvs_diff_init,
	cvs_diff_pre_exec,
	cvs_diff_remote,
	cvs_diff_local,
	NULL,
	cvs_diff_cleanup,
	CVS_CMD_SENDARGS2 | CVS_CMD_ALLOWSPEC | CVS_CMD_SENDDIR
};


struct cvs_cmd cvs_cmd_rdiff = {
	CVS_OP_RDIFF, CVS_REQ_DIFF, "rdiff",
	{ "pa", "patch" },
	"Create 'patch' format diffs between releases",
	"[-flR] [-c | -u] [-s | -t] [-V ver] -D date | -r rev "
	"[-D date2 | -rev2] module ...",
	"cD:flRr:stuV:",
	NULL,
	CF_RECURSE | CF_IGNORE | CF_SORT | CF_KNOWN,
	cvs_diff_init,
	cvs_diff_pre_exec,
	cvs_diff_remote,
	cvs_diff_local,
	NULL,
	cvs_diff_cleanup,
	CVS_CMD_SENDARGS2 | CVS_CMD_ALLOWSPEC | CVS_CMD_SENDDIR
};
#endif

#if !defined(RCSPROG)
static struct diff_arg *dap = NULL;
static int recurse;

static int
cvs_diff_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch;

	dap = (struct diff_arg *)malloc(sizeof(*dap));
	if (dap == NULL)
		return (CVS_EX_DATA);
	dap->date1 = dap->date2 = dap->rev1 = dap->rev2 = NULL;
	strlcpy(diffargs, argv[0], sizeof(diffargs));

	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
		case 'c':
			strlcat(diffargs, " -c", sizeof(diffargs));
			format = D_CONTEXT;
			break;
		case 'D':
			if (dap->date1 == NULL && dap->rev1 == NULL) {
				dap->date1 = optarg;
			} else if (dap->date2 == NULL && dap->rev2 == NULL) {
				dap->date2 = optarg;
			} else {
				cvs_log(LP_ERR,
				    "no more than two revisions/dates can "
				    "be specified");
			}
			break;
		case 'l':
			strlcat(diffargs, " -l", sizeof(diffargs));
			recurse = 0;
			cvs_cmd_diff.file_flags &= ~CF_RECURSE;
			break;
		case 'i':
			strlcat(diffargs, " -i", sizeof(diffargs));
			iflag = 1;
			break;
		case 'N':
			strlcat(diffargs, " -N", sizeof(diffargs));
			Nflag = 1;
			break;
		case 'n':
			strlcat(diffargs, " -n", sizeof(diffargs));
			format = D_RCSDIFF;
			break;
		case 'p':
			strlcat(diffargs, " -p", sizeof(diffargs));
			pflag = 1;
			break;
		case 'r':
			if ((dap->rev1 == NULL) && (dap->date1 == NULL)) {
				dap->rev1 = optarg;
			} else if ((dap->rev2 == NULL) &&
			    (dap->date2 == NULL)) {
				dap->rev2 = optarg;
			} else {
				cvs_log(LP_ERR,
				    "no more than two revisions/dates can "
				    "be specified");
				return (CVS_EX_USAGE);
			}
			break;
		case 'R':
			cvs_cmd_diff.file_flags |= CF_RECURSE;
			break;
		case 'u':
			strlcat(diffargs, " -u", sizeof(diffargs));
			format = D_UNIFIED;
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	*arg = optind;
	return (0);
}

int
cvs_diff_cleanup(void)
{
	if (dap != NULL) {
		free(dap);
		dap = NULL;
	}
	return (0);
}

/*
 * cvs_diff_pre_exec()
 *
 */
int
cvs_diff_pre_exec(struct cvsroot *root)
{
	if (root->cr_method != CVS_METHOD_LOCAL) {
		/* send the flags */
		if (Nflag && (cvs_sendarg(root, "-N", 0) < 0))
			return (CVS_EX_PROTO);
		if (pflag && (cvs_sendarg(root, "-p", 0) < 0))
			return (CVS_EX_PROTO);

		if (format == D_CONTEXT) {
			if (cvs_sendarg(root, "-c", 0) < 0)
				return (CVS_EX_PROTO);
		} else if (format == D_UNIFIED) {
			if (cvs_sendarg(root, "-u", 0) < 0)
				return (CVS_EX_PROTO);
		}

		if (dap->rev1 != NULL) {
			if ((cvs_sendarg(root, "-r", 0) < 0) ||
			    (cvs_sendarg(root, dap->rev1, 0) < 0))
				return (CVS_EX_PROTO);
		} else if (dap->date1 != NULL) {
			if ((cvs_sendarg(root, "-D", 0) < 0) ||
			    (cvs_sendarg(root, dap->date1, 0) < 0))
				return (CVS_EX_PROTO);
		}
		if (dap->rev2 != NULL) {
			if ((cvs_sendarg(root, "-r", 0) < 0) ||
			    (cvs_sendarg(root, dap->rev2, 0) < 0))
				return (CVS_EX_PROTO);
		} else if (dap->date2 != NULL) {
			if ((cvs_sendarg(root, "-D", 0) < 0) ||
			    (cvs_sendarg(root, dap->date2, 0) < 0))
				return  (CVS_EX_PROTO);
		}
	}

	return (0);
}


/*
 * cvs_diff_file()
 *
 * Diff a single file.
 */
static int
cvs_diff_remote(struct cvs_file *cfp, void *arg)
{
	char *dir, *repo;
	char fpath[MAXPATHLEN], dfpath[MAXPATHLEN];
	struct cvsroot *root;

	if (cfp->cf_type == DT_DIR) {
		if (cfp->cf_cvstat == CVS_FST_UNKNOWN) {
			root = cfp->cf_parent->cf_root;
			cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cfp->cf_name);
		} else {
			root = cfp->cf_root;
#if 0
			if ((cfp->cf_parent == NULL) ||
			    (root != cfp->cf_parent->cf_root)) {
				cvs_connect(root);
				cvs_diff_pre_exec(root);
			}
#endif

			cvs_senddir(root, cfp);
		}

		return (0);
	}

	if (cfp->cf_cvstat == CVS_FST_LOST) {
		cvs_log(LP_WARN, "cannot find file %s", cfp->cf_name);
		return (0);
	}

	diff_file = cvs_file_getpath(cfp, fpath, sizeof(fpath));

	if (cfp->cf_parent != NULL) {
		dir = cvs_file_getpath(cfp->cf_parent, dfpath, sizeof(dfpath));
		root = cfp->cf_parent->cf_root;
		repo = cfp->cf_parent->cf_repo;
	} else {
		dir = ".";
		root = NULL;
		repo = NULL;
	}

	if (cfp->cf_cvstat == CVS_FST_UNKNOWN) {
		cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cfp->cf_name);
		return (0);
	}

	if (cvs_sendentry(root, cfp) < 0)
		return (CVS_EX_PROTO);

	if (cfp->cf_cvstat == CVS_FST_UPTODATE) {
		cvs_sendreq(root, CVS_REQ_UNCHANGED, cfp->cf_name);
		return (0);
	}

	/* at this point, the file is modified */
	if ((cvs_sendreq(root, CVS_REQ_MODIFIED, cfp->cf_name) < 0) ||
	    (cvs_sendfile(root, diff_file) < 0))
		return (CVS_EX_PROTO);

	return (0);
}

static int
cvs_diff_local(CVSFILE *cf, void *arg)
{
	int len;
	char *repo, buf[64];
	char fpath[MAXPATHLEN], rcspath[MAXPATHLEN];
	char path_tmp1[MAXPATHLEN], path_tmp2[MAXPATHLEN];
	BUF *b1, *b2;
	RCSNUM *r1, *r2;
	RCSFILE *rf;
	struct cvsroot *root;

	rf = NULL;
	root = CVS_DIR_ROOT(cf);
	repo = CVS_DIR_REPO(cf);
	diff_file = cvs_file_getpath(cf, fpath, sizeof(fpath));

	if (cf->cf_type == DT_DIR) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "Diffing %s", fpath);
		return (0);
	}

	if (cf->cf_cvstat == CVS_FST_LOST) {
		cvs_log(LP_WARN, "cannot find file %s", cf->cf_name);
		return (0);
	}

	if (cf->cf_cvstat == CVS_FST_UNKNOWN) {
		cvs_log(LP_WARN, "I know nothing about %s", diff_file);
		return (0);
	} else if (cf->cf_cvstat == CVS_FST_UPTODATE)
		return (0);

	/* at this point, the file is modified */
	len = snprintf(rcspath, sizeof(rcspath), "%s/%s/%s%s",
	    root->cr_dir, repo, diff_file, RCS_FILE_EXT);
	if (len == -1 || len >= (int)sizeof(rcspath)) {
		errno = ENAMETOOLONG;
		cvs_log(LP_ERRNO, "%s", rcspath);
		return (CVS_EX_DATA);
	}

	rf = rcs_open(rcspath, RCS_READ);
	if (rf == NULL) {
		return (CVS_EX_DATA);
	}

	cvs_printf("Index: %s\n%s\nRCS file: %s\n", diff_file,
	    RCS_DIFF_DIV, rcspath);

	if (dap->rev1 == NULL)
		r1 = cf->cf_lrev;
	else {
		if ((r1 = rcsnum_parse(dap->rev1)) == NULL) {
			return (CVS_EX_DATA);
		}
	}

	cvs_printf("retrieving revision %s\n",
	    rcsnum_tostr(r1, buf, sizeof(buf)));
	b1 = rcs_getrev(rf, r1);

	if (b1 == NULL) {
		cvs_log(LP_ERR, "failed to retrieve revision %s\n",
		    rcsnum_tostr(r1, buf, sizeof(buf)));
		if (r1 != cf->cf_lrev)
			rcsnum_free(r1);
		return (CVS_EX_DATA);
	}

	if (r1 != cf->cf_lrev)
		rcsnum_free(r1);

	if (dap->rev2 != NULL) {
		cvs_printf("retrieving revision %s\n", dap->rev2);
		if ((r2 = rcsnum_parse(dap->rev2)) == NULL) {
			return (CVS_EX_DATA);
		}
		b2 = rcs_getrev(rf, r2);
		rcsnum_free(r2);
	} else {
		b2 = cvs_buf_load(diff_file, BUF_AUTOEXT);
	}

	rcs_close(rf);

	if (b2 == NULL) {
		cvs_log(LP_ERR, "failed to retrieve revision %s\n",
		    dap->rev2);
		cvs_buf_free(b1);
		return (CVS_EX_DATA);
	}

	cvs_printf("%s", diffargs);
	cvs_printf(" -r%s", buf);
	if (dap->rev2 != NULL)
		cvs_printf(" -r%s", dap->rev2);
	cvs_printf(" %s\n", diff_file);
	strlcpy(path_tmp1, "/tmp/diff1.XXXXXXXXXX", sizeof(path_tmp1));
	if (cvs_buf_write_stmp(b1, path_tmp1, 0600) == -1) {
		cvs_buf_free(b1);
		cvs_buf_free(b2);
		return (CVS_EX_DATA);
	}
	cvs_buf_free(b1);

	strlcpy(path_tmp2, "/tmp/diff2.XXXXXXXXXX", sizeof(path_tmp2));
	if (cvs_buf_write_stmp(b2, path_tmp2, 0600) == -1) {
		cvs_buf_free(b2);
		(void)unlink(path_tmp1);
		return (CVS_EX_DATA);
	}
	cvs_buf_free(b2);

	cvs_diffreg(path_tmp1, path_tmp2);
	(void)unlink(path_tmp1);
	(void)unlink(path_tmp2);

	return (0);
}
#endif


int
cvs_diffreg(const char *file1, const char *file2)
{
	FILE *f1, *f2;
	int i, rval;
	void *tmp;

	f1 = f2 = NULL;
	rval = D_SAME;
	anychange = 0;
	lastline = 0;
	lastmatchline = 0;
	context_vec_ptr = context_vec_start - 1;
	chrtran = (iflag ? cup2low : clow2low);

	f1 = fopen(file1, "r");
	if (f1 == NULL) {
		cvs_log(LP_ERRNO, "%s", file1);
		goto closem;
	}

	f2 = fopen(file2, "r");
	if (f2 == NULL) {
		cvs_log(LP_ERRNO, "%s", file2);
		goto closem;
	}

	switch (files_differ(f1, f2)) {
	case 0:
		goto closem;
	case 1:
		break;
	default:
		/* error */
		goto closem;
	}

	if (!asciifile(f1) || !asciifile(f2)) {
		rval = D_BINARY;
		goto closem;
	}
	if ((prepare(0, f1, stb1.st_size) < 0) ||
	    (prepare(1, f2, stb2.st_size) < 0)) {
		goto closem;
	}
	prune();
	sort(sfile[0], slen[0]);
	sort(sfile[1], slen[1]);

	member = (int *)file[1];
	equiv(sfile[0], slen[0], sfile[1], slen[1], member);
	if ((tmp = realloc(member, (slen[1] + 2) * sizeof(int))) == NULL) {
		free(member);
		member = NULL;
		cvs_log(LP_ERRNO, "failed to resize member");
		goto closem;
	}
	member = (int *)tmp;

	class = (int *)file[0];
	unsort(sfile[0], slen[0], class);
	if ((tmp = realloc(class, (slen[0] + 2) * sizeof(int))) == NULL) {
		free(class);
		class = NULL;
		cvs_log(LP_ERRNO, "failed to resize class");
		goto closem;
	}
	class = (int *)tmp;

	if ((klist = malloc((slen[0] + 2) * sizeof(int))) == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate klist");
		goto closem;
	}
	clen = 0;
	clistlen = 100;
	if ((clist = malloc(clistlen * sizeof(cand))) == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate clist");
		goto closem;
	}

	if ((i = stone(class, slen[0], member, klist)) < 0)
		goto closem;

	free(member);
	free(class);

	if ((tmp = realloc(J, (diff_len[0] + 2) * sizeof(int))) == NULL) {
		free(J);
		J = NULL;
		cvs_log(LP_ERRNO, "failed to resize J");
		goto closem;
	}
	J = (int *)tmp;
	unravel(klist[i]);
	free(clist);
	free(klist);

	if ((tmp = realloc(ixold, (diff_len[0] + 2) * sizeof(long))) == NULL) {
		free(ixold);
		ixold = NULL;
		cvs_log(LP_ERRNO, "failed to resize ixold");
		goto closem;
	}
	ixold = (long *)tmp;
	if ((tmp = realloc(ixnew, (diff_len[1] + 2) * sizeof(long))) == NULL) {
		free(ixnew);
		ixnew = NULL;
		cvs_log(LP_ERRNO, "failed to resize ixnew");
		goto closem;
	}
	ixnew = (long *)tmp;
	check(f1, f2);
	output(file1, f1, file2, f2);

closem:
	if (anychange) {
		if (rval == D_SAME)
			rval = D_DIFFER;
	}
	if (f1 != NULL)
		fclose(f1);
	if (f2 != NULL)
		fclose(f2);

	return (rval);
}

/*
 * Check to see if the given files differ.
 * Returns 0 if they are the same, 1 if different, and -1 on error.
 * XXX - could use code from cmp(1) [faster]
 */
static int
files_differ(FILE *f1, FILE *f2)
{
	char buf1[BUFSIZ], buf2[BUFSIZ];
	size_t i, j;

	if (stb1.st_size != stb2.st_size)
		return (1);
	for (;;) {
		i = fread(buf1, (size_t)1, sizeof(buf1), f1);
		j = fread(buf2, (size_t)1, sizeof(buf2), f2);
		if (i != j)
			return (1);
		if (i == 0 && j == 0) {
			if (ferror(f1) || ferror(f2))
				return (1);
			return (0);
		}
		if (memcmp(buf1, buf2, i) != 0)
			return (1);
	}
}

static int
prepare(int i, FILE *fd, off_t filesize)
{
	void *tmp;
	struct line *p;
	int j, h;
	size_t sz;

	rewind(fd);

	sz = ((size_t)filesize <= SIZE_MAX ? (size_t)filesize : SIZE_MAX) / 25;
	if (sz < 100)
		sz = 100;

	p = (struct line *)malloc((sz + 3) * sizeof(struct line));
	if (p == NULL) {
		cvs_log(LP_ERRNO, "failed to prepare line array");
		return (-1);
	}
	for (j = 0; (h = readhash(fd));) {
		if (j == (int)sz) {
			sz = sz * 3 / 2;
			tmp = realloc(p, (sz + 3) * sizeof(struct line));
			if (tmp == NULL) {
				cvs_log(LP_ERRNO, "failed to grow line array");
				free(p);
				return (-1);
			}
			p = (struct line *)tmp;
		}
		p[++j].value = h;
	}
	diff_len[i] = j;
	file[i] = p;

	return (0);
}

static void
prune(void)
{
	int i, j;

	for (pref = 0; pref < diff_len[0] && pref < diff_len[1] &&
	    file[0][pref + 1].value == file[1][pref + 1].value;
	    pref++)
		;
	for (suff = 0;
	    (suff < diff_len[0] - pref) && (suff < diff_len[1] - pref) &&
	    (file[0][diff_len[0] - suff].value ==
	    file[1][diff_len[1] - suff].value);
	    suff++)
		;
	for (j = 0; j < 2; j++) {
		sfile[j] = file[j] + pref;
		slen[j] = diff_len[j] - pref - suff;
		for (i = 0; i <= slen[j]; i++)
			sfile[j][i].serial = i;
	}
}

static void
equiv(struct line *a, int n, struct line *b, int m, int *c)
{
	int i, j;

	i = j = 1;
	while (i <= n && j <= m) {
		if (a[i].value < b[j].value)
			a[i++].value = 0;
		else if (a[i].value == b[j].value)
			a[i++].value = j;
		else
			j++;
	}
	while (i <= n)
		a[i++].value = 0;
	b[m + 1].value = 0;
	j = 0;
	while (++j <= m) {
		c[j] = -b[j].serial;
		while (b[j + 1].value == b[j].value) {
			j++;
			c[j] = b[j].serial;
		}
	}
	c[j] = -1;
}

/* Code taken from ping.c */
static int
isqrt(int n)
{
	int y, x = 1;

	if (n == 0)
		return (0);

	do { /* newton was a stinker */
		y = x;
		x = n / x;
		x += y;
		x /= 2;
	} while ((x - y) > 1 || (x - y) < -1);

	return (x);
}

static int
stone(int *a, int n, int *b, int *c)
{
	int ret;
	int i, k, y, j, l;
	int oldc, tc, oldl;
	u_int numtries;

	/* XXX move the isqrt() out of the macro to avoid multiple calls */
	const u_int bound = dflag ? UINT_MAX : MAX(256, (u_int)isqrt(n));

	k = 0;
	if ((ret = newcand(0, 0, 0)) < 0)
		return (-1);
	c[0] = ret;
	for (i = 1; i <= n; i++) {
		j = a[i];
		if (j == 0)
			continue;
		y = -b[j];
		oldl = 0;
		oldc = c[0];
		numtries = 0;
		do {
			if (y <= clist[oldc].y)
				continue;
			l = search(c, k, y);
			if (l != oldl + 1)
				oldc = c[l - 1];
			if (l <= k) {
				if (clist[c[l]].y <= y)
					continue;
				tc = c[l];
				if ((ret = newcand(i, y, oldc)) < 0)
					return (-1);
				c[l] = ret;
				oldc = tc;
				oldl = l;
				numtries++;
			} else {
				if ((ret = newcand(i, y, oldc)) < 0)
					return (-1);
				c[l] = ret;
				k++;
				break;
			}
		} while ((y = b[++j]) > 0 && numtries < bound);
	}
	return (k);
}

static int
newcand(int x, int y, int pred)
{
	struct cand *q, *tmp;
	int newclistlen;

	if (clen == clistlen) {
		newclistlen = clistlen * 11 / 10;
		tmp = realloc(clist, newclistlen * sizeof(cand));
		if (tmp == NULL) {
			cvs_log(LP_ERRNO, "failed to resize clist");
			return (-1);
		}
		clist = tmp;
		clistlen = newclistlen;
	}
	q = clist + clen;
	q->x = x;
	q->y = y;
	q->pred = pred;
	return (clen++);
}

static int
search(int *c, int k, int y)
{
	int i, j, l, t;

	if (clist[c[k]].y < y)	/* quick look for typical case */
		return (k + 1);
	i = 0;
	j = k + 1;
	while (1) {
		l = i + j;
		if ((l >>= 1) <= i)
			break;
		t = clist[c[l]].y;
		if (t > y)
			j = l;
		else if (t < y)
			i = l;
		else
			return (l);
	}
	return (l + 1);
}

static void
unravel(int p)
{
	struct cand *q;
	int i;

	for (i = 0; i <= diff_len[0]; i++)
		J[i] = i <= pref ? i :
		    i > diff_len[0] - suff ? i + diff_len[1] - diff_len[0] : 0;
	for (q = clist + p; q->y != 0; q = clist + q->pred)
		J[q->x + pref] = q->y + pref;
}

/*
 * Check does double duty:
 *  1.	ferret out any fortuitous correspondences due
 *	to confounding by hashing (which result in "jackpot")
 *  2.  collect random access indexes to the two files
 */
static void
check(FILE *f1, FILE *f2)
{
	int i, j, jackpot, c, d;
	long ctold, ctnew;

	rewind(f1);
	rewind(f2);
	j = 1;
	ixold[0] = ixnew[0] = 0;
	jackpot = 0;
	ctold = ctnew = 0;
	for (i = 1; i <= diff_len[0]; i++) {
		if (J[i] == 0) {
			ixold[i] = ctold += skipline(f1);
			continue;
		}
		while (j < J[i]) {
			ixnew[j] = ctnew += skipline(f2);
			j++;
		}
		if (bflag || wflag || iflag) {
			for (;;) {
				c = getc(f1);
				d = getc(f2);
				/*
				 * GNU diff ignores a missing newline
				 * in one file if bflag || wflag.
				 */
				if ((bflag || wflag) &&
				    ((c == EOF && d == '\n') ||
				    (c == '\n' && d == EOF))) {
					break;
				}
				ctold++;
				ctnew++;
				if (bflag && isspace(c) && isspace(d)) {
					do {
						if (c == '\n')
							break;
						ctold++;
					} while (isspace(c = getc(f1)));
					do {
						if (d == '\n')
							break;
						ctnew++;
					} while (isspace(d = getc(f2)));
				} else if (wflag) {
					while (isspace(c) && c != '\n') {
						c = getc(f1);
						ctold++;
					}
					while (isspace(d) && d != '\n') {
						d = getc(f2);
						ctnew++;
					}
				}
				if (chrtran[c] != chrtran[d]) {
					jackpot++;
					J[i] = 0;
					if (c != '\n' && c != EOF)
						ctold += skipline(f1);
					if (d != '\n' && c != EOF)
						ctnew += skipline(f2);
					break;
				}
				if (c == '\n' || c == EOF)
					break;
			}
		} else {
			for (;;) {
				ctold++;
				ctnew++;
				if ((c = getc(f1)) != (d = getc(f2))) {
					/* jackpot++; */
					J[i] = 0;
					if (c != '\n' && c != EOF)
						ctold += skipline(f1);
					if (d != '\n' && c != EOF)
						ctnew += skipline(f2);
					break;
				}
				if (c == '\n' || c == EOF)
					break;
			}
		}
		ixold[i] = ctold;
		ixnew[j] = ctnew;
		j++;
	}
	for (; j <= diff_len[1]; j++)
		ixnew[j] = ctnew += skipline(f2);
	/*
	 * if (jackpot)
	 *	cvs_printf("jackpot\n");
	 */
}

/* shellsort CACM #201 */
static void
sort(struct line *a, int n)
{
	struct line *ai, *aim, w;
	int j, m = 0, k;

	if (n == 0)
		return;
	for (j = 1; j <= n; j *= 2)
		m = 2 * j - 1;
	for (m /= 2; m != 0; m /= 2) {
		k = n - m;
		for (j = 1; j <= k; j++) {
			for (ai = &a[j]; ai > a; ai -= m) {
				aim = &ai[m];
				if (aim < ai)
					break;	/* wraparound */
				if (aim->value > ai[0].value ||
				    (aim->value == ai[0].value &&
					aim->serial > ai[0].serial))
					break;
				w.value = ai[0].value;
				ai[0].value = aim->value;
				aim->value = w.value;
				w.serial = ai[0].serial;
				ai[0].serial = aim->serial;
				aim->serial = w.serial;
			}
		}
	}
}

static void
unsort(struct line *f, int l, int *b)
{
	int *a, i;

	if ((a = (int *)malloc((l + 1) * sizeof(int))) == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate sort array");
		return;
	}
	for (i = 1; i <= l; i++)
		a[f[i].serial] = f[i].value;
	for (i = 1; i <= l; i++)
		b[i] = a[i];
	free(a);
}

static int
skipline(FILE *f)
{
	int i, c;

	for (i = 1; (c = getc(f)) != '\n' && c != EOF; i++)
		continue;
	return (i);
}

static void
output(const char *file1, FILE *f1, const char *file2, FILE *f2)
{
	int m, i0, i1, j0, j1;

	rewind(f1);
	rewind(f2);
	m = diff_len[0];
	J[0] = 0;
	J[m + 1] = diff_len[1] + 1;
	for (i0 = 1; i0 <= m; i0 = i1 + 1) {
		while (i0 <= m && J[i0] == J[i0 - 1] + 1)
			i0++;
		j0 = J[i0 - 1] + 1;
		i1 = i0 - 1;
		while (i1 < m && J[i1 + 1] == 0)
			i1++;
		j1 = J[i1 + 1] - 1;
		J[i1] = j1;
		change(file1, f1, file2, f2, i0, i1, j0, j1);
	}
	if (m == 0)
		change(file1, f1, file2, f2, 1, 0, 1, diff_len[1]);
	if (format == D_IFDEF) {
		for (;;) {
#define	c i0
			if ((c = getc(f1)) == EOF)
				return;
			cvs_putchar(c);
		}
#undef c
	}
	if (anychange != 0) {
		if (format == D_CONTEXT)
			dump_context_vec(f1, f2);
		else if (format == D_UNIFIED)
			dump_unified_vec(f1, f2);
	}
}

static __inline void
range(int a, int b, char *separator)
{
	cvs_printf("%d", a > b ? b : a);
	if (a < b)
		cvs_printf("%s%d", separator, b);
}

static __inline void
uni_range(int a, int b)
{
	if (a < b)
		cvs_printf("%d,%d", a, b - a + 1);
	else if (a == b)
		cvs_printf("%d", b);
	else
		cvs_printf("%d,0", b);
}

static char *
preadline(int fd, size_t rlen, off_t off)
{
	char *line;
	ssize_t nr;

	line = malloc(rlen + 1);
	if (line == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate line");
		return (NULL);
	}
	if ((nr = pread(fd, line, rlen, off)) < 0) {
		cvs_log(LP_ERRNO, "preadline failed");
		return (NULL);
	}
	line[nr] = '\0';
	return (line);
}

static int
ignoreline(char *line)
{
	int ret;

	ret = regexec(&ignore_re, line, (size_t)0, NULL, 0);
	free(line);
	return (ret == 0);	/* if it matched, it should be ignored. */
}

/*
 * Indicate that there is a difference between lines a and b of the from file
 * to get to lines c to d of the to file.  If a is greater then b then there
 * are no lines in the from file involved and this means that there were
 * lines appended (beginning at b).  If c is greater than d then there are
 * lines missing from the to file.
 */
static void
change(const char *file1, FILE *f1, const char *file2, FILE *f2,
	int a, int b, int c, int d)
{
	static size_t max_context = 64;
	int i;

	if (format != D_IFDEF && a > b && c > d)
		return;
	if (ignore_pats != NULL) {
		char *line;
		/*
		 * All lines in the change, insert, or delete must
		 * match an ignore pattern for the change to be
		 * ignored.
		 */
		if (a <= b) {		/* Changes and deletes. */
			for (i = a; i <= b; i++) {
				line = preadline(fileno(f1),
				    ixold[i] - ixold[i - 1], ixold[i - 1]);
				if (!ignoreline(line))
					goto proceed;
			}
		}
		if (a > b || c <= d) {	/* Changes and inserts. */
			for (i = c; i <= d; i++) {
				line = preadline(fileno(f2),
				    ixnew[i] - ixnew[i - 1], ixnew[i - 1]);
				if (!ignoreline(line))
					goto proceed;
			}
		}
		return;
	}
proceed:
	if (format == D_CONTEXT || format == D_UNIFIED) {
		/*
		 * Allocate change records as needed.
		 */
		if (context_vec_ptr == context_vec_end - 1) {
			struct context_vec *tmp;
			ptrdiff_t offset = context_vec_ptr - context_vec_start;
			max_context <<= 1;
			if ((tmp = realloc(context_vec_start,
			    max_context * sizeof(struct context_vec))) == NULL) {
				free(context_vec_start);
				context_vec_start = NULL;
				cvs_log(LP_ERRNO,
				    "failed to resize context_vec_start");
				return;
			}
			context_vec_start = tmp;
			context_vec_end = context_vec_start + max_context;
			context_vec_ptr = context_vec_start + offset;
		}
		if (anychange == 0) {
			/*
			 * Print the context/unidiff header first time through.
			 */
			cvs_printf("%s %s	%s",
			    format == D_CONTEXT ? "***" : "---", diff_file,
			    ctime(&stb1.st_mtime));
			cvs_printf("%s %s	%s",
			    format == D_CONTEXT ? "---" : "+++", diff_file,
			    ctime(&stb2.st_mtime));
			anychange = 1;
		} else if (a > context_vec_ptr->b + (2 * context) + 1 &&
		    c > context_vec_ptr->d + (2 * context) + 1) {
			/*
			 * If this change is more than 'context' lines from the
			 * previous change, dump the record and reset it.
			 */
			if (format == D_CONTEXT)
				dump_context_vec(f1, f2);
			else
				dump_unified_vec(f1, f2);
		}
		context_vec_ptr++;
		context_vec_ptr->a = a;
		context_vec_ptr->b = b;
		context_vec_ptr->c = c;
		context_vec_ptr->d = d;
		return;
	}
	if (anychange == 0)
		anychange = 1;
	switch (format) {
	case D_BRIEF:
		return;
	case D_NORMAL:
		range(a, b, ",");
		cvs_putchar(a > b ? 'a' : c > d ? 'd' : 'c');
		if (format == D_NORMAL)
			range(c, d, ",");
		cvs_putchar('\n');
		break;
	case D_RCSDIFF:
		if (a > b)
			cvs_printf("a%d %d\n", b, d - c + 1);
		else {
			cvs_printf("d%d %d\n", a, b - a + 1);

			if (!(c > d))	/* add changed lines */
				cvs_printf("a%d %d\n", b, d - c + 1);
		}
		break;
	}
	if (format == D_NORMAL || format == D_IFDEF) {
		fetch(ixold, a, b, f1, '<', 1);
		if (a <= b && c <= d && format == D_NORMAL)
			puts("---");
	}
	i = fetch(ixnew, c, d, f2, format == D_NORMAL ? '>' : '\0', 0);
	if (inifdef) {
		cvs_printf("#endif /* %s */\n", ifdefname);
		inifdef = 0;
	}
}

static int
fetch(long *f, int a, int b, FILE *lb, int ch, int oldfile)
{
	int i, j, c, lastc, col, nc;

	/*
	 * When doing #ifdef's, copy down to current line
	 * if this is the first file, so that stuff makes it to output.
	 */
	if (format == D_IFDEF && oldfile) {
		long curpos = ftell(lb);
		/* print through if append (a>b), else to (nb: 0 vs 1 orig) */
		nc = f[a > b ? b : a - 1] - curpos;
		for (i = 0; i < nc; i++)
			cvs_putchar(getc(lb));
	}
	if (a > b)
		return (0);
	if (format == D_IFDEF) {
		if (inifdef) {
			cvs_printf("#else /* %s%s */\n",
			    oldfile == 1 ? "!" : "", ifdefname);
		} else {
			if (oldfile)
				cvs_printf("#ifndef %s\n", ifdefname);
			else
				cvs_printf("#ifdef %s\n", ifdefname);
		}
		inifdef = 1 + oldfile;
	}
	for (i = a; i <= b; i++) {
		fseek(lb, f[i - 1], SEEK_SET);
		nc = f[i] - f[i - 1];
		if (format != D_IFDEF && ch != '\0') {
			cvs_putchar(ch);
			if (Tflag && (format == D_NORMAL || format == D_CONTEXT
			    || format == D_UNIFIED))
				cvs_putchar('\t');
			else if (format != D_UNIFIED)
				cvs_putchar(' ');
		}
		col = 0;
		for (j = 0, lastc = '\0'; j < nc; j++, lastc = c) {
			if ((c = getc(lb)) == EOF) {
				if (format == D_RCSDIFF)
					warnx("No newline at end of file");
				else
					puts("\n\\ No newline at end of file");
				return (0);
			}
			if (c == '\t' && tflag) {
				do {
					cvs_putchar(' ');
				} while (++col & 7);
			} else {
				cvs_putchar(c);
				col++;
			}
		}
	}
	return (0);
}

/*
 * Hash function taken from Robert Sedgewick, Algorithms in C, 3d ed., p 578.
 */
static int
readhash(FILE *f)
{
	int i, t, space;
	int sum;

	sum = 1;
	space = 0;
	if (!bflag && !wflag) {
		if (iflag)
			for (i = 0; (t = getc(f)) != '\n'; i++) {
				if (t == EOF) {
					if (i == 0)
						return (0);
					break;
				}
				sum = sum * 127 + chrtran[t];
			}
		else
			for (i = 0; (t = getc(f)) != '\n'; i++) {
				if (t == EOF) {
					if (i == 0)
						return (0);
					break;
				}
				sum = sum * 127 + t;
			}
	} else {
		for (i = 0;;) {
			switch (t = getc(f)) {
			case '\t':
			case ' ':
				space++;
				continue;
			default:
				if (space && !wflag) {
					i++;
					space = 0;
				}
				sum = sum * 127 + chrtran[t];
				i++;
				continue;
			case EOF:
				if (i == 0)
					return (0);
				/* FALLTHROUGH */
			case '\n':
				break;
			}
			break;
		}
	}
	/*
	 * There is a remote possibility that we end up with a zero sum.
	 * Zero is used as an EOF marker, so return 1 instead.
	 */
	return (sum == 0 ? 1 : sum);
}

static int
asciifile(FILE *f)
{
	char buf[BUFSIZ];
	int i, cnt;

	if (aflag || f == NULL)
		return (1);

	rewind(f);
	cnt = fread(buf, (size_t)1, sizeof(buf), f);
	for (i = 0; i < cnt; i++)
		if (!isprint(buf[i]) && !isspace(buf[i]))
			return (0);
	return (1);
}

static char*
match_function(const long *f, int pos, FILE *fp)
{
	unsigned char buf[FUNCTION_CONTEXT_SIZE];
	size_t nc;
	int last = lastline;
	char *p;

	lastline = pos;
	while (pos > last) {
		fseek(fp, f[pos - 1], SEEK_SET);
		nc = f[pos] - f[pos - 1];
		if (nc >= sizeof(buf))
			nc = sizeof(buf) - 1;
		nc = fread(buf, (size_t)1, nc, fp);
		if (nc > 0) {
			buf[nc] = '\0';
			p = strchr((const char *)buf, '\n');
			if (p != NULL)
				*p = '\0';
			if (isalpha(buf[0]) || buf[0] == '_' || buf[0] == '$') {
				strlcpy(lastbuf, (const char *)buf, sizeof lastbuf);
				lastmatchline = pos;
				return lastbuf;
			}
		}
		pos--;
	}
	return (lastmatchline > 0) ? lastbuf : NULL;
}


/* dump accumulated "context" diff changes */
static void
dump_context_vec(FILE *f1, FILE *f2)
{
	struct context_vec *cvp = context_vec_start;
	int lowa, upb, lowc, upd, do_output;
	int a, b, c, d;
	char ch, *f;

	if (context_vec_start > context_vec_ptr)
		return;

	b = d = 0;		/* gcc */
	lowa = MAX(1, cvp->a - context);
	upb = MIN(diff_len[0], context_vec_ptr->b + context);
	lowc = MAX(1, cvp->c - context);
	upd = MIN(diff_len[1], context_vec_ptr->d + context);

	cvs_printf("***************");
	if (pflag) {
		f = match_function(ixold, lowa - 1, f1);
		if (f != NULL) {
			cvs_putchar(' ');
			cvs_printf("%s", f);
		}
	}
	cvs_printf("\n*** ");
	range(lowa, upb, ",");
	cvs_printf(" ****\n");

	/*
	 * Output changes to the "old" file.  The first loop suppresses
	 * output if there were no changes to the "old" file (we'll see
	 * the "old" lines as context in the "new" list).
	 */
	do_output = 0;
	for (; cvp <= context_vec_ptr; cvp++)
		if (cvp->a <= cvp->b) {
			cvp = context_vec_start;
			do_output++;
			break;
		}
	if (do_output) {
		while (cvp <= context_vec_ptr) {
			a = cvp->a;
			b = cvp->b;
			c = cvp->c;
			d = cvp->d;

			if (a <= b && c <= d)
				ch = 'c';
			else
				ch = (a <= b) ? 'd' : 'a';

			if (ch == 'a')
				fetch(ixold, lowa, b, f1, ' ', 0);
			else {
				fetch(ixold, lowa, a - 1, f1, ' ', 0);
				fetch(ixold, a, b, f1,
				    ch == 'c' ? '!' : '-', 0);
			}
			lowa = b + 1;
			cvp++;
		}
		fetch(ixold, b + 1, upb, f1, ' ', 0);
	}
	/* output changes to the "new" file */
	cvs_printf("--- ");
	range(lowc, upd, ",");
	cvs_printf(" ----\n");

	do_output = 0;
	for (cvp = context_vec_start; cvp <= context_vec_ptr; cvp++)
		if (cvp->c <= cvp->d) {
			cvp = context_vec_start;
			do_output++;
			break;
		}
	if (do_output) {
		while (cvp <= context_vec_ptr) {
			a = cvp->a;
			b = cvp->b;
			c = cvp->c;
			d = cvp->d;

			if (a <= b && c <= d)
				ch = 'c';
			else
				ch = (a <= b) ? 'd' : 'a';

			if (ch == 'd')
				fetch(ixnew, lowc, d, f2, ' ', 0);
			else {
				fetch(ixnew, lowc, c - 1, f2, ' ', 0);
				fetch(ixnew, c, d, f2,
				    ch == 'c' ? '!' : '+', 0);
			}
			lowc = d + 1;
			cvp++;
		}
		fetch(ixnew, d + 1, upd, f2, ' ', 0);
	}
	context_vec_ptr = context_vec_start - 1;
}

/* dump accumulated "unified" diff changes */
static void
dump_unified_vec(FILE *f1, FILE *f2)
{
	struct context_vec *cvp = context_vec_start;
	int lowa, upb, lowc, upd;
	int a, b, c, d;
	char ch, *f;

	if (context_vec_start > context_vec_ptr)
		return;

	b = d = 0;		/* gcc */
	lowa = MAX(1, cvp->a - context);
	upb = MIN(diff_len[0], context_vec_ptr->b + context);
	lowc = MAX(1, cvp->c - context);
	upd = MIN(diff_len[1], context_vec_ptr->d + context);

	cvs_printf("@@ -");
	uni_range(lowa, upb);
	cvs_printf(" +");
	uni_range(lowc, upd);
	cvs_printf(" @@");
	if (pflag) {
		f = match_function(ixold, lowa - 1, f1);
		if (f != NULL) {
			cvs_putchar(' ');
			cvs_printf("%s", f);
		}
	}
	cvs_putchar('\n');

	/*
	 * Output changes in "unified" diff format--the old and new lines
	 * are printed together.
	 */
	for (; cvp <= context_vec_ptr; cvp++) {
		a = cvp->a;
		b = cvp->b;
		c = cvp->c;
		d = cvp->d;

		/*
		 * c: both new and old changes
		 * d: only changes in the old file
		 * a: only changes in the new file
		 */
		if (a <= b && c <= d)
			ch = 'c';
		else
			ch = (a <= b) ? 'd' : 'a';

		switch (ch) {
		case 'c':
			fetch(ixold, lowa, a - 1, f1, ' ', 0);
			fetch(ixold, a, b, f1, '-', 0);
			fetch(ixnew, c, d, f2, '+', 0);
			break;
		case 'd':
			fetch(ixold, lowa, a - 1, f1, ' ', 0);
			fetch(ixold, a, b, f1, '-', 0);
			break;
		case 'a':
			fetch(ixnew, lowc, c - 1, f2, ' ', 0);
			fetch(ixnew, c, d, f2, '+', 0);
			break;
		}
		lowa = b + 1;
		lowc = d + 1;
	}
	fetch(ixnew, d + 1, upd, f2, ' ', 0);

	context_vec_ptr = context_vec_start - 1;
}
