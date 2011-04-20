/*	$OpenBSD: diff.c,v 1.160 2011/04/20 18:33:13 nicm Exp $	*/
/*
 * Copyright (c) 2008 Tobias Stoeckmann <tobias@openbsd.org>
 * Copyright (c) 2006 Joris Vink <joris@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/stat.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cvs.h"
#include "diff.h"
#include "remote.h"

void	cvs_diff_local(struct cvs_file *);

static int	 dflags = 0;
static int	 Nflag = 0;
static int	 force_head = 0;
static char	*koptstr;
static char	*rev1 = NULL;
static char	*rev2 = NULL;
static time_t	 date1 = -1;
static time_t	 date2 = -1;
static char	*dateflag1 = NULL;
static char	*dateflag2 = NULL;

struct cvs_cmd cvs_cmd_diff = {
	CVS_OP_DIFF, CVS_USE_WDIR, "diff",
	{ "di", "dif" },
	"Show differences between revisions",
	"[-abcdilNnpRuw] [[-D date] [-r rev] [-D date2 | -r rev2]] "
	"[-k mode] [file ...]",
	"abcfC:dD:ik:lNnpr:RuU:w",
	NULL,
	cvs_diff
};

struct cvs_cmd cvs_cmd_rdiff = {
	CVS_OP_RDIFF, 0, "rdiff",
	{ "patch", "pa" },
	"Show differences between revisions",
	"[-flR] [-c | -u] [-s | -t] [-V ver] -D date | -r rev\n"
	"[-D date2 | -r rev2] [-k mode] module ...",
	"cfD:k:lr:RuV:",
	NULL,
	cvs_diff
};

int
cvs_diff(int argc, char **argv)
{
	int ch, flags;
	char *arg = ".";
	const char *errstr;
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;
	strlcpy(diffargs, cvs_cmdop == CVS_OP_DIFF ? "diff" : "rdiff",
	    sizeof(diffargs));

	while ((ch = getopt(argc, argv, cvs_cmdop == CVS_OP_DIFF ?
	    cvs_cmd_diff.cmd_opts : cvs_cmd_rdiff.cmd_opts)) != -1) {
		switch (ch) {
		case 'a':
			strlcat(diffargs, " -a", sizeof(diffargs));
			dflags |= D_FORCEASCII;
			break;
		case 'b':
			strlcat(diffargs, " -b", sizeof(diffargs));
			dflags |= D_FOLDBLANKS;
			break;
		case 'c':
			strlcat(diffargs, " -c", sizeof(diffargs));
			diff_format = D_CONTEXT;
			break;
		case 'C':
			diff_context = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				fatal("context lines %s: %s", errstr, optarg);
			strlcat(diffargs, " -C ", sizeof(diffargs));
			strlcat(diffargs, optarg, sizeof(diffargs));
			diff_format = D_CONTEXT;
			break;
		case 'd':
			strlcat(diffargs, " -d", sizeof(diffargs));
			dflags |= D_MINIMAL;
			break;
		case 'D':
			if (date1 == -1 && rev1 == NULL) {
				if ((date1 = date_parse(optarg)) == -1)
					fatal("invalid date: %s", optarg);
				dateflag1 = optarg;
			} else if (date2 == -1 && rev2 == NULL) {
				if ((date2 = date_parse(optarg)) == -1)
					fatal("invalid date: %s", optarg);
				dateflag2 = optarg;
			} else {
				fatal("no more than 2 revisions/dates can"
				    " be specified");
			}
			break;
		case 'f':
			force_head = 1;
			break;
		case 'i':
			strlcat(diffargs, " -i", sizeof(diffargs));
			dflags |= D_IGNORECASE;
			break;
		case 'k':
			koptstr = optarg;
			kflag = rcs_kflag_get(koptstr);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid RCS keyword expansion mode");
				fatal("%s", cvs_cmdop == CVS_OP_DIFF ?
				    cvs_cmd_diff.cmd_synopsis :
				    cvs_cmd_rdiff.cmd_synopsis);
			}
			break;
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'n':
			strlcat(diffargs, " -n", sizeof(diffargs));
			diff_format = D_RCSDIFF;
			break;
		case 'N':
			strlcat(diffargs, " -N", sizeof(diffargs));
			Nflag = 1;
			break;
		case 'p':
			strlcat(diffargs, " -p", sizeof(diffargs));
			dflags |= D_PROTOTYPE;
			break;
		case 'R':
			flags |= CR_RECURSE_DIRS;
			break;
		case 'r':
			if (date1 == -1 && rev1 == NULL) {
				rev1 = optarg;
			} else if (date2 == -1 && rev2 == NULL) {
				rev2 = optarg;
			} else {
				fatal("no more than 2 revisions/dates can"
				    " be specified");
			}
			break;
		case 't':
			strlcat(diffargs, " -t", sizeof(diffargs));
			dflags |= D_EXPANDTABS;
			break;
		case 'u':
			strlcat(diffargs, " -u", sizeof(diffargs));
			diff_format = D_UNIFIED;
			break;
		case 'U':
			diff_context = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				fatal("context lines %s: %s", errstr, optarg);
			strlcat(diffargs, " -U ", sizeof(diffargs));
			strlcat(diffargs, optarg, sizeof(diffargs));
			diff_format = D_UNIFIED;
			break;
		case 'V':
			fatal("the -V option is obsolete "
			    "and should not be used");
		case 'w':
			strlcat(diffargs, " -w", sizeof(diffargs));
			dflags |= D_IGNOREBLANKS;
			break;
		default:
			fatal("%s", cvs_cmdop == CVS_OP_DIFF ?
			    cvs_cmd_diff.cmd_synopsis :
			    cvs_cmd_rdiff.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	cr.enterdir = NULL;
	cr.leavedir = NULL;

	if (cvs_cmdop == CVS_OP_RDIFF) {
		if (rev1 == NULL && rev2 == NULL && dateflag1 == NULL &&
		    dateflag2 == NULL)
			fatal("must specify at least one revision/date!");

		if (!argc)
			fatal("%s", cvs_cmd_rdiff.cmd_synopsis);

		if (!diff_format) {
			strlcat(diffargs, " -c", sizeof(diffargs));
			diff_format = D_CONTEXT;
		}

		flags |= CR_REPO;
	}

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();
		cr.fileproc = cvs_client_sendfile;

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");

		if (kflag)
			cvs_client_send_request("Argument -k%s", koptstr);

		switch (diff_format) {
		case D_CONTEXT:
			if (cvs_cmdop == CVS_OP_RDIFF)
				cvs_client_send_request("Argument -c");
			else {
				cvs_client_send_request("Argument -C %d",
				    diff_context);
			}
			break;
		case D_RCSDIFF:
			cvs_client_send_request("Argument -n");
			break;
		case D_UNIFIED:
			if (cvs_cmdop == CVS_OP_RDIFF || diff_context == 3)
				cvs_client_send_request("Argument -u");
			else {
				cvs_client_send_request("Argument -U %d",
				    diff_context);
			}
			break;
		default:
			break;
		}

		if (Nflag == 1)
			cvs_client_send_request("Argument -N");

		if (dflags & D_PROTOTYPE)
			cvs_client_send_request("Argument -p");

		if (rev1 != NULL)
			cvs_client_send_request("Argument -r%s", rev1);
		if (rev2 != NULL)
			cvs_client_send_request("Argument -r%s", rev2);

		if (dateflag1 != NULL)
			cvs_client_send_request("Argument -D%s", dateflag1);
		if (dateflag2 != NULL)
			cvs_client_send_request("Argument -D%s", dateflag2);
	} else {
		if (cvs_cmdop == CVS_OP_RDIFF &&
		    chdir(current_cvsroot->cr_dir) == -1)
			fatal("cvs_diff: %s", strerror(errno));

		cr.fileproc = cvs_diff_local;
	}

	cr.flags = flags;

	diff_rev1 = diff_rev2 = NULL;

	if (cvs_cmdop == CVS_OP_DIFF ||
	    current_cvsroot->cr_method == CVS_METHOD_LOCAL) {
		if (argc > 0)
			cvs_file_run(argc, argv, &cr);
		else
			cvs_file_run(1, &arg, &cr);
	}

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");

		cvs_client_send_request((cvs_cmdop == CVS_OP_RDIFF) ?
		    "rdiff" : "diff");

		cvs_client_get_responses();
	}

	return (0);
}

void
cvs_diff_local(struct cvs_file *cf)
{
	BUF *b1;
	int fd1, fd2;
	struct stat st;
	struct timeval tv[2], tv2[2];
	struct tm datetm;
	char rbuf[CVS_REV_BUFSZ], tbuf[CVS_TIME_BUFSZ], *p1, *p2;

	b1 = NULL;
	fd1 = fd2 = -1;
	p1 = p2 = NULL;

	cvs_log(LP_TRACE, "cvs_diff_local(%s)", cf->file_path);

	if (cf->file_type == CVS_DIR) {
		if (verbosity > 1)
			cvs_log(LP_ERR, "Diffing inside %s", cf->file_path);
		return;
	}

	cvs_file_classify(cf, cvs_directory_tag);

	if (cvs_cmdop == CVS_OP_DIFF) {
		if (cf->file_ent == NULL) {
			cvs_log(LP_ERR, "I know nothing about %s",
			    cf->file_path);
			return;
		}

		switch (cf->file_ent->ce_status) {
		case CVS_ENT_ADDED:
			if (Nflag == 0) {
				cvs_log(LP_ERR, "%s is a new entry, no "
				    "comparison available", cf->file_path);
				return;
			}
			if (!(cf->file_flags & FILE_ON_DISK)) {
				cvs_log(LP_ERR, "cannot find %s",
				    cf->file_path);
				return;
			}
			break;
		case CVS_ENT_REMOVED:
			if (Nflag == 0) {
				cvs_log(LP_ERR, "%s was removed, no "
				    "comparison available", cf->file_path);
				return;
			}
			if (cf->file_rcs == NULL) {
				cvs_log(LP_ERR, "cannot find RCS file for %s",
				    cf->file_path);
				return;
			}
			break;
		default:
			if (!(cf->file_flags & FILE_ON_DISK)) {
				cvs_printf("? %s\n", cf->file_path);
				return;
			}

			if (cf->file_rcs == NULL) {
				cvs_log(LP_ERR, "cannot find RCS file for %s",
				    cf->file_path);
				return;
			}
			break;
		}
	}

	if (cf->file_status == FILE_UPTODATE && rev1 == NULL && rev2 == NULL &&
	    date1 == -1 && date2 == -1)
		return;

	if (cf->file_rcs != NULL && cf->file_rcs->rf_head == NULL) {
		cvs_log(LP_ERR, "no head revision in RCS file for %s\n",
		    cf->file_path);
		return;
	}

	if (kflag && cf->file_rcs != NULL)
		rcs_kwexp_set(cf->file_rcs, kflag);

	if (cf->file_rcs == NULL)
		diff_rev1 = NULL;
	else if (rev1 != NULL || date1 != -1) {
		cvs_specified_date = date1;
		diff_rev1 = rcs_translate_tag(rev1, cf->file_rcs);
		if (diff_rev1 == NULL && cvs_cmdop == CVS_OP_DIFF) {
			if (rev1 != NULL) {
				cvs_log(LP_ERR, "tag %s not in file %s", rev1,
				    cf->file_path);
				goto cleanup;
			} else if (Nflag) {
				diff_rev1 = NULL;
			} else {
				gmtime_r(&cvs_specified_date, &datetm);
				strftime(tbuf, sizeof(tbuf),
				    "%Y.%m.%d.%H.%M.%S", &datetm);
				cvs_log(LP_ERR, "no revision for date %s in "
				    "file %s", tbuf, cf->file_path);
				goto cleanup;
			}
		} else if (diff_rev1 == NULL && cvs_cmdop == CVS_OP_RDIFF &&
		    force_head) {
			/* -f is not allowed for unknown symbols */
			if ((diff_rev1 = rcsnum_parse(rev1)) == NULL)
				fatal("no such tag %s", rev1);
			rcsnum_free(diff_rev1);

			diff_rev1 = cf->file_rcs->rf_head;
		}
		cvs_specified_date = -1;
	} else if (cvs_cmdop == CVS_OP_DIFF) {
		if (cf->file_ent->ce_status == CVS_ENT_ADDED)
			diff_rev1 = NULL;
		else
			diff_rev1 = cf->file_ent->ce_rev;
	}

	if (cf->file_rcs == NULL)
		diff_rev2 = NULL;
	else if (rev2 != NULL || date2 != -1) {
		cvs_specified_date = date2;
		diff_rev2 = rcs_translate_tag(rev2, cf->file_rcs);
		if (diff_rev2 == NULL && cvs_cmdop == CVS_OP_DIFF) {
			if (rev2 != NULL) {
				cvs_log(LP_ERR, "tag %s not in file %s", rev2,
				    cf->file_path);
				goto cleanup;
			} else if (Nflag) {
				diff_rev2 = NULL;
			} else {
				gmtime_r(&cvs_specified_date, &datetm);
				strftime(tbuf, sizeof(tbuf),
				    "%Y.%m.%d.%H.%M.%S", &datetm);
				cvs_log(LP_ERR, "no revision for date %s in "
				    "file %s", tbuf, cf->file_path);
				goto cleanup;
			}
		} else if (diff_rev2 == NULL && cvs_cmdop == CVS_OP_RDIFF &&
		    force_head) {
			/* -f is not allowed for unknown symbols */
			if ((diff_rev2 = rcsnum_parse(rev2)) == NULL)
				fatal("no such tag %s", rev2);
			rcsnum_free(diff_rev2);

			diff_rev2 = cf->file_rcs->rf_head;
		}
		cvs_specified_date = -1;
	} else if (cvs_cmdop == CVS_OP_RDIFF)
		diff_rev2 = cf->file_rcs->rf_head;
	else if (cf->file_ent->ce_status == CVS_ENT_REMOVED)
		diff_rev2 = NULL;

	if (diff_rev1 != NULL && diff_rev2 != NULL &&
	    rcsnum_cmp(diff_rev1, diff_rev2, 0) == 0)
		goto cleanup;

	switch (cvs_cmdop) {
	case CVS_OP_DIFF:
		if (cf->file_status == FILE_UPTODATE) {
			if (diff_rev2 == NULL &&
			    !rcsnum_cmp(diff_rev1, cf->file_rcsrev, 0))
				goto cleanup;
		}
		break;
	case CVS_OP_RDIFF:
		if (diff_rev1 == NULL && diff_rev2 == NULL)
			goto cleanup;
		break;
	}

	cvs_printf("Index: %s\n", cf->file_path);
	if (cvs_cmdop == CVS_OP_DIFF)
		cvs_printf("%s\nRCS file: %s\n", RCS_DIFF_DIV,
		    cf->file_rcs != NULL ? cf->file_rpath : cf->file_path);

	if (diff_rev1 != NULL) {
		if (cvs_cmdop == CVS_OP_DIFF && diff_rev1 != NULL) {
			(void)rcsnum_tostr(diff_rev1, rbuf, sizeof(rbuf));
			cvs_printf("retrieving revision %s\n", rbuf);
		}

		tv[0].tv_sec = rcs_rev_getdate(cf->file_rcs, diff_rev1);
		tv[0].tv_usec = 0;
		tv[1] = tv[0];

		(void)xasprintf(&p1, "%s/diff1.XXXXXXXXXX", cvs_tmpdir);
		fd1 = rcs_rev_write_stmp(cf->file_rcs, diff_rev1, p1, 0);
		if (futimes(fd1, tv) == -1)
			fatal("cvs_diff_local: utimes failed");
	}

	if (diff_rev2 != NULL) {
		if (cvs_cmdop == CVS_OP_DIFF && rev2 != NULL) {
			(void)rcsnum_tostr(diff_rev2, rbuf, sizeof(rbuf));
			cvs_printf("retrieving revision %s\n", rbuf);
		}

		tv2[0].tv_sec = rcs_rev_getdate(cf->file_rcs, diff_rev2);
		tv2[0].tv_usec = 0;
		tv2[1] = tv2[0];

		(void)xasprintf(&p2, "%s/diff2.XXXXXXXXXX", cvs_tmpdir);
		fd2 = rcs_rev_write_stmp(cf->file_rcs, diff_rev2, p2, 0);
		if (futimes(fd2, tv2) == -1)
			fatal("cvs_diff_local: utimes failed");
	} else if (cvs_cmdop == CVS_OP_DIFF &&
	    (cf->file_flags & FILE_ON_DISK) &&
	    cf->file_ent->ce_status != CVS_ENT_REMOVED) {
		(void)xasprintf(&p2, "%s/diff2.XXXXXXXXXX", cvs_tmpdir);
		if (cvs_server_active == 1 && cf->fd == -1) {
			tv2[0].tv_sec = rcs_rev_getdate(cf->file_rcs,
			    cf->file_ent->ce_rev);
			tv2[0].tv_usec = 0;
			tv2[1] = tv2[0];

			fd2 = rcs_rev_write_stmp(cf->file_rcs,
			    cf->file_ent->ce_rev, p2, 0);
			if (futimes(fd2, tv2) == -1)
				fatal("cvs_diff_local: futimes failed");
		} else {
			if (fstat(cf->fd, &st) == -1)
				fatal("fstat failed %s", strerror(errno));
			b1 = buf_load_fd(cf->fd);

			tv2[0].tv_sec = st.st_mtime;
			tv2[0].tv_usec = 0;
			tv2[1] = tv2[0];

			fd2 = buf_write_stmp(b1, p2, tv2);
			buf_free(b1);
		}
	}

	switch (cvs_cmdop) {
	case CVS_OP_DIFF:
		cvs_printf("%s", diffargs);

		if (rev1 != NULL && diff_rev1 != NULL) {
			(void)rcsnum_tostr(diff_rev1, rbuf, sizeof(rbuf));
			cvs_printf(" -r%s", rbuf);

			if (rev2 != NULL && diff_rev2 != NULL) {
				(void)rcsnum_tostr(diff_rev2, rbuf,
				    sizeof(rbuf));
				cvs_printf(" -r%s", rbuf);
			}
		}

		if (diff_rev2 == NULL)
			cvs_printf(" %s", cf->file_path);
		cvs_printf("\n");
		break;
	case CVS_OP_RDIFF:
		cvs_printf("diff ");
		switch (diff_format) {
		case D_CONTEXT:
			cvs_printf("-c ");
			break;
		case D_RCSDIFF:
			cvs_printf("-n ");
			break;
		case D_UNIFIED:
			cvs_printf("-u ");
			break;
		default:
			break;
		}
		if (diff_rev1 == NULL) {
			cvs_printf("%s ", CVS_PATH_DEVNULL);
		} else {
			(void)rcsnum_tostr(diff_rev1, rbuf, sizeof(rbuf));
			cvs_printf("%s:%s ", cf->file_path, rbuf);
		}

		if (diff_rev2 == NULL) {
			cvs_printf("%s:removed\n", cf->file_path);
		} else {
			(void)rcsnum_tostr(diff_rev2 != NULL ? diff_rev2 :
			    cf->file_rcs->rf_head, rbuf, sizeof(rbuf));
			cvs_printf("%s:%s\n", cf->file_path, rbuf);
		}
		break;
	}

	if (fd1 == -1) {
		if ((fd1 = open(CVS_PATH_DEVNULL, O_RDONLY, 0)) == -1)
			fatal("cannot open %s", CVS_PATH_DEVNULL);
	}
	if (fd2 == -1) {
		if ((fd2 = open(CVS_PATH_DEVNULL, O_RDONLY, 0)) == -1)
			fatal("cannot open %s", CVS_PATH_DEVNULL);
	}

	if (diffreg(p1 != NULL ? cf->file_path : CVS_PATH_DEVNULL,
	    p2 != NULL ? cf->file_path : CVS_PATH_DEVNULL, fd1, fd2, NULL,
	    dflags) == D_ERROR)
		fatal("cvs_diff_local: failed to get RCS patch");

	close(fd1);
	close(fd2);

	worklist_run(&temp_files, worklist_unlink);

	if (p1 != NULL)
		xfree(p1);
	if (p2 != NULL)
		xfree(p2);

cleanup:
	if (diff_rev1 != NULL &&
	    (cf->file_rcs == NULL || diff_rev1 != cf->file_rcs->rf_head) &&
	    (cf->file_ent == NULL || diff_rev1 != cf->file_ent->ce_rev))
		xfree(diff_rev1);
	diff_rev1 = NULL;

	if (diff_rev2 != NULL &&
	    (cf->file_rcs == NULL || diff_rev2 != cf->file_rcs->rf_head))
		xfree(diff_rev2);
	diff_rev2 = NULL;
}
