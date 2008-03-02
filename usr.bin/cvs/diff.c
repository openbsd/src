/*	$OpenBSD: diff.c,v 1.133 2008/03/02 19:05:34 tobias Exp $	*/
/*
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
#include <unistd.h>

#include "cvs.h"
#include "diff.h"
#include "remote.h"

void	cvs_diff_local(struct cvs_file *);

static int	 Nflag = 0;
static int	 force_head = 0;
static char	*koptstr;
static char	*rev1 = NULL;
static char	*rev2 = NULL;

struct cvs_cmd cvs_cmd_diff = {
	CVS_OP_DIFF, CVS_USE_WDIR, "diff",
	{ "di", "dif" },
	"Show differences between revisions",
	"[-cilNnpRu] [[-D date] [-r rev] [-D date2 | -r rev2]] "
	"[-k mode] [file ...]",
	"cfD:ik:lNnpr:Ru",
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
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;
	strlcpy(diffargs, cvs_cmdop == CVS_OP_DIFF ? "diff" : "rdiff",
	    sizeof(diffargs));

	while ((ch = getopt(argc, argv, cvs_cmdop == CVS_OP_DIFF ?
	    cvs_cmd_diff.cmd_opts : cvs_cmd_rdiff.cmd_opts)) != -1) {
		switch (ch) {
		case 'c':
			strlcat(diffargs, " -c", sizeof(diffargs));
			diff_format = D_CONTEXT;
			break;
		case 'f':
			force_head = 1;
			break;
		case 'i':
			strlcat(diffargs, " -i", sizeof(diffargs));
			diff_iflag = 1;
			break;
		case 'k':
			koptstr = optarg;
			kflag = rcs_kflag_get(koptstr);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid RCS keyword expension mode");
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
			Nflag = 1;
			break;
		case 'p':
			strlcat(diffargs, " -p", sizeof(diffargs));
			diff_pflag = 1;
			break;
		case 'R':
			flags |= CR_RECURSE_DIRS;
			break;
		case 'r':
			if (rev1 == NULL) {
				rev1 = optarg;
			} else if (rev2 == NULL) {
				rev2 = optarg;
			} else {
				fatal("no more than 2 revisions/dates can"
				    " be specified");
			}
			break;
		case 'u':
			strlcat(diffargs, " -u", sizeof(diffargs));
			diff_format = D_UNIFIED;
			break;
		case 'V':
			fatal("the -V option is obsolete "
			    "and should not be used");
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
		if (rev1 == NULL)
			fatal("must specify at least one revision/date!");

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
			cvs_client_send_request("Argument -c");
			break;
		case D_RCSDIFF:
			cvs_client_send_request("Argument -n");
			break;
		case D_UNIFIED:
			cvs_client_send_request("Argument -u");
			break;
		default:
			break;
		}

		if (Nflag == 1)
			cvs_client_send_request("Argument -N");

		if (diff_pflag == 1)
			cvs_client_send_request("Argument -p");

		if (rev1 != NULL)
			cvs_client_send_request("Argument -r%s", rev1);
		if (rev2 != NULL)
			cvs_client_send_request("Argument -r%s", rev2);
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
	RCSNUM *r1;
	BUF *b1;
	int fd1, fd2;
	struct stat st;
	struct timeval tv[2], tv2[2];
	char rbuf[CVS_REV_BUFSZ], *p1, *p2;

	r1 = NULL;
	b1 = NULL;

	cvs_log(LP_TRACE, "cvs_diff_local(%s)", cf->file_path);

	if (cf->file_type == CVS_DIR) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "Diffing inside %s", cf->file_path);
		return;
	}

	cvs_file_classify(cf, cvs_directory_tag);

	if (cf->file_status == FILE_LOST) {
		cvs_log(LP_ERR, "cannot find file %s", cf->file_path);
		return;
	} else if (cf->file_status == FILE_UNKNOWN) {
		cvs_log(LP_ERR, "I know nothing about %s", cf->file_path);
		return;
	} else if (cf->file_status == FILE_ADDED && Nflag == 0) {
		cvs_log(LP_ERR, "%s is a new entry, no comparison available",
		    cf->file_path);
		return;
	} else if (cf->file_status == FILE_REMOVED && Nflag == 0) {
		cvs_log(LP_ERR, "%s was removed, no comparison available",
		    cf->file_path);
		return;
	} else if (cf->file_status == FILE_UPTODATE && rev1 == NULL &&
	     rev2 == NULL) {
		return;
	}

	if (kflag)
		rcs_kwexp_set(cf->file_rcs, kflag);

	if (rev1 != NULL)
		if ((diff_rev1 = rcs_translate_tag(rev1, cf->file_rcs)) ==
		    NULL) {
			if (cvs_cmdop == CVS_OP_DIFF) {
				cvs_log(LP_ERR, "tag %s is not in file %s",
				    rev1, cf->file_path);
				return;
			}
			if (force_head) {
				/* -f is not allowed for unknown symbols */
				diff_rev1 = rcsnum_parse(rev1);
				if (diff_rev1 == NULL)
					fatal("no such tag %s", rev1);
				rcsnum_free(diff_rev1);

				diff_rev1 = rcsnum_alloc();
				rcsnum_cpy(cf->file_rcs->rf_head, diff_rev1, 0);
			}
		}

	if (rev2 != NULL)
		if ((diff_rev2 = rcs_translate_tag(rev2, cf->file_rcs)) ==
		    NULL) {
			if (cvs_cmdop == CVS_OP_DIFF) {
				rcsnum_free(diff_rev1);
				cvs_log(LP_ERR, "tag %s is not in file %s",
				    rev2, cf->file_path);
				return;
			}
			if (force_head) {
				/* -f is not allowed for unknown symbols */
				diff_rev2 = rcsnum_parse(rev2);
				if (diff_rev2 == NULL)
					fatal("no such tag %s", rev2);
				rcsnum_free(diff_rev2);

				diff_rev2 = rcsnum_alloc();
				rcsnum_cpy(cf->file_rcs->rf_head, diff_rev2, 0);
			}
		}

	if (cvs_cmdop == CVS_OP_RDIFF && diff_rev1 == NULL && diff_rev2 == NULL)
		return;

	diff_file = cf->file_path;

	cvs_printf("Index: %s\n", cf->file_path);
	if (cvs_cmdop == CVS_OP_DIFF)
		cvs_printf("%s\nRCS file: %s\n", RCS_DIFF_DIV, cf->file_rpath);

	(void)xasprintf(&p1, "%s/diff1.XXXXXXXXXX", cvs_tmpdir);
	(void)xasprintf(&p2, "%s/diff2.XXXXXXXXXX", cvs_tmpdir);

	if (cf->file_status != FILE_ADDED) {
		if (diff_rev1 != NULL)
			r1 = diff_rev1;
		else if (cf->file_ent != NULL)
			r1 = cf->file_ent->ce_rev;
		else
			r1 = NULL;

		diff_rev1 = r1;

		if (diff_rev1 != NULL) {
			(void)rcsnum_tostr(r1, rbuf, sizeof(rbuf));

			tv[0].tv_sec = rcs_rev_getdate(cf->file_rcs, r1);
			tv[0].tv_usec = 0;
			tv[1] = tv[0];

			if (cvs_cmdop == CVS_OP_DIFF)
				cvs_printf("retrieving revision %s\n", rbuf);
			fd1 = rcs_rev_write_stmp(cf->file_rcs, r1, p1, 0);
			if (futimes(fd1, tv) == -1)
				fatal("cvs_diff_local: utimes failed");
		}
	}

	if (diff_rev2 != NULL && cf->file_status != FILE_ADDED &&
	    cf->file_status != FILE_REMOVED) {
		(void)rcsnum_tostr(diff_rev2, rbuf, sizeof(rbuf));

		tv2[0].tv_sec = rcs_rev_getdate(cf->file_rcs, diff_rev2);
		tv2[0].tv_usec = 0;
		tv2[1] = tv2[0];

		if (cvs_cmdop == CVS_OP_DIFF)
			cvs_printf("retrieving revision %s\n", rbuf);
		fd2 = rcs_rev_write_stmp(cf->file_rcs, diff_rev2, p2, 0);
		if (futimes(fd2, tv2) == -1)
			fatal("cvs_diff_local: utimes failed");
	} else if (cf->file_status != FILE_REMOVED) {
		if (cvs_cmdop == CVS_OP_RDIFF || (cvs_server_active == 1 &&
		    cf->file_status != FILE_MODIFIED)) {
			if (diff_rev2 != NULL) {
				tv2[0].tv_sec = rcs_rev_getdate(cf->file_rcs,
				    cf->file_rcsrev);
				tv2[0].tv_usec = 0;
				tv2[1] = tv2[0];

				fd2 = rcs_rev_write_stmp(cf->file_rcs,
				    cf->file_rcsrev, p2, 0);
				if (futimes(fd2, tv2) == -1)
					fatal("cvs_diff_local: utimes failed");
			}
		} else {
			if (fstat(cf->fd, &st) == -1)
				fatal("fstat failed %s", strerror(errno));
			b1 = cvs_buf_load_fd(cf->fd);

			tv2[0].tv_sec = st.st_mtime;
			tv2[0].tv_usec = 0;
			tv2[1] = tv2[0];

			fd2 = cvs_buf_write_stmp(b1, p2, tv2);
			cvs_buf_free(b1);
		}
	}

	if (cvs_cmdop == CVS_OP_DIFF) {
		cvs_printf("%s", diffargs);

		if (cf->file_status != FILE_ADDED) {
			(void)rcsnum_tostr(r1, rbuf, sizeof(rbuf));
			cvs_printf(" -r%s", rbuf);

			if (diff_rev2 != NULL) {
				(void)rcsnum_tostr(diff_rev2, rbuf,
				    sizeof(rbuf));
				cvs_printf(" -r%s", rbuf);
			}
		}

		if (diff_rev2 == NULL)
			cvs_printf(" %s", cf->file_name);
		cvs_printf("\n");
	} else {
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
	}

	if (cf->file_status == FILE_ADDED ||
	    (cvs_cmdop == CVS_OP_RDIFF && diff_rev1 == NULL)) {
		xfree(p1);
		close(fd1);
		(void)xasprintf(&p1, "%s", CVS_PATH_DEVNULL);
		if ((fd1 = open(p1, O_RDONLY)) == -1)
			fatal("cvs_diff_local: cannot open %s",
			    CVS_PATH_DEVNULL);
	} else if (cf->file_status == FILE_REMOVED ||
	    (cvs_cmdop == CVS_OP_RDIFF && diff_rev2 == NULL)) {
		xfree(p2);
		close(fd2);
		(void)xasprintf(&p2, "%s", CVS_PATH_DEVNULL);
		if ((fd1 = open(p2, O_RDONLY)) == -1)
			fatal("cvs_diff_local: cannot open %s",
			    CVS_PATH_DEVNULL);
	}

	if (cvs_diffreg(p1, p2, fd1, fd2, NULL) == D_ERROR)
		fatal("cvs_diff_local: failed to get RCS patch");

	close(fd1);
	close(fd2);

	cvs_worklist_run(&temp_files, cvs_worklist_unlink);

	if (p1 != NULL)
		xfree(p1);
	if (p2 != NULL)
		xfree(p2);

	if (diff_rev1 != NULL && diff_rev1 != cf->file_rcs->rf_head &&
	    (cf->file_ent != NULL && diff_rev1 != cf->file_ent->ce_rev))
		rcsnum_free(diff_rev1);
	if (diff_rev2 != NULL)
		rcsnum_free(diff_rev2);

	diff_rev1 = diff_rev2 = NULL;
}
