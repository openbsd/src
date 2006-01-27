/*	$OpenBSD: add.c,v 1.39 2006/01/27 12:45:21 xsa Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * Copyright (c) 2005 Xavier Santolaria <xsa@openbsd.org>
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
#include "proto.h"


extern char *__progname;


static int	cvs_add_remote(CVSFILE *, void *);
static int	cvs_add_local(CVSFILE *, void *);
static int	cvs_add_init(struct cvs_cmd *, int, char **, int *);
static int	cvs_add_pre_exec(struct cvsroot *);
static int	cvs_add_directory(CVSFILE *);
static int	cvs_add_build_entry(CVSFILE *);

struct cvs_cmd cvs_cmd_add = {
	CVS_OP_ADD, CVS_REQ_ADD, "add",
	{ "ad", "new" },
	"Add a new file/directory to the repository",
	"[-k mode] [-m msg] file ...",
	"k:m:",
	NULL,
	0,
	cvs_add_init,
	cvs_add_pre_exec,
	cvs_add_remote,
	cvs_add_local,
	NULL,
	NULL,
	CVS_CMD_ALLOWSPEC | CVS_CMD_SENDDIR | CVS_CMD_SENDARGS2
};

static int kflag = RCS_KWEXP_DEFAULT;
static char *koptstr;
static char kbuf[16];

static int
cvs_add_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch;

	cvs_msg = NULL;

	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
		case 'k':
			koptstr = optarg;
			kflag = rcs_kflag_get(koptstr);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid RCS keyword expansion mode");
				rcs_kflag_usage();
				return (CVS_EX_USAGE);
			}
			break;
		case 'm':
			cvs_msg = xstrdup(optarg);
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	*arg = optind;
	return (0);
}

static int
cvs_add_pre_exec(struct cvsroot *root)
{
	kbuf[0] = '\0';

	if (kflag != RCS_KWEXP_DEFAULT) {
		strlcpy(kbuf, "-k", sizeof(kbuf));
		strlcat(kbuf, koptstr, sizeof(kbuf));

		if (root->cr_method != CVS_METHOD_LOCAL)
			cvs_sendarg(root, kbuf, 0);
	}

	return (0);
}

static int
cvs_add_remote(CVSFILE *cf, void *arg)
{
	struct cvsroot *root;

	root = CVS_DIR_ROOT(cf);

	if (cf->cf_type == DT_DIR) {
		cvs_senddir(root, cf);
		return (0);
	}

	if (cf->cf_cvstat == CVS_FST_UNKNOWN)
		cvs_sendreq(root, CVS_REQ_ISMODIFIED, cf->cf_name);

	return (0);
}

static int
cvs_add_local(CVSFILE *cf, void *arg)
{
	int added, ret;
	char numbuf[64];

	added = 0;

	/* dont use `cvs add *' */
	if ((strcmp(cf->cf_name, ".") == 0) ||
	    (strcmp(cf->cf_name, "..") == 0) ||
	    (strcmp(cf->cf_name, CVS_PATH_CVSDIR) == 0)) {
		if (verbosity > 1)
			fatal("cannot add special file `%s'.", cf->cf_name);
	}

	if (cf->cf_type == DT_DIR)
		return cvs_add_directory(cf);

	if ((!(cf->cf_flags & CVS_FILE_ONDISK)) &&
	    (cf->cf_cvstat != CVS_FST_LOST) &&
	    (cf->cf_cvstat != CVS_FST_REMOVED)) {
		if (verbosity > 1)
			cvs_log(LP_WARN, "nothing known about `%s'",
			    cf->cf_name);
		return (0);
	} else if (cf->cf_cvstat == CVS_FST_ADDED) {
		if (verbosity > 1)
			cvs_log(LP_WARN, "`%s' has already been entered",
			    cf->cf_name);
		return (0);
	} else if (cf->cf_cvstat == CVS_FST_REMOVED) {

		/* XXX remove '-' from CVS/Entries */

		/* XXX check the file out */

		rcsnum_tostr(cf->cf_lrev, numbuf, sizeof(numbuf));
		cvs_log(LP_WARN, "%s, version %s, resurrected",
		    cf->cf_name, numbuf);

		return (0);
	} else if ((cf->cf_cvstat == CVS_FST_CONFLICT) ||
	    (cf->cf_cvstat == CVS_FST_LOST) ||
	    (cf->cf_cvstat == CVS_FST_MODIFIED) ||
	    (cf->cf_cvstat == CVS_FST_UPTODATE)) {
		if (verbosity > 1) {
			rcsnum_tostr(cf->cf_lrev, numbuf, sizeof(numbuf));
			cvs_log(LP_WARN,
			    "%s already exists, with version number %s",
			    cf->cf_name, numbuf);
		}
		return (0);
	}

	if ((ret = cvs_add_build_entry(cf)) != 0)
		return (ret);
	else {
		added++;
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "scheduling file `%s' for addition",
			    cf->cf_name);
	}

	if (added != 0) {
		if (verbosity > 0)
			cvs_log(LP_NOTICE, "use '%s commit' to add %s "
			    "permanently", __progname,
			    (added == 1) ? "this file" : "these files");
		return (0);
	}

	return (0);
}

/*
 * cvs_add_directory()
 *
 * Add a directory to the repository.
 *
 * Returns 0 on success, -1 on failure.
 */
static int
cvs_add_directory(CVSFILE *cf)
{
	int nb;
	char *date, *repo, *tag;
	char entry[CVS_ENT_MAXLINELEN], fpath[MAXPATHLEN], rcsdir[MAXPATHLEN];
	char msg[1024];
	CVSENTRIES *entf;
	struct cvsroot *root;
	struct stat st;
	struct cvs_ent *ent;

	entf = (CVSENTRIES *)cf->cf_entry;

	root = CVS_DIR_ROOT(cf);
	repo = CVS_DIR_REPO(cf);

	if (strlcpy(fpath, cf->cf_name, sizeof(fpath)) >= sizeof(fpath))
		fatal("cvs_add_directory: path truncation");

	if (strchr(fpath, '/') != NULL)
		fatal("directory %s not added; must be a direct sub-directory",
		    fpath);

	/* Let's see if we have any per-directory tags first */
	cvs_parse_tagfile(&tag, &date, &nb);

	/* XXX check for <dir>/CVS */

	if (strlcpy(rcsdir, root->cr_dir, sizeof(rcsdir)) >= sizeof(rcsdir) ||
	    strlcat(rcsdir, "/", sizeof(rcsdir)) >= sizeof(rcsdir) ||
	    strlcat(rcsdir, repo, sizeof(rcsdir)) >= sizeof(rcsdir))
		fatal("cvs_add_directory: path truncation");

	if ((stat(rcsdir, &st) == 0) && !(S_ISDIR(st.st_mode)))
		fatal("%s is not a directory; %s not added: %s", rcsdir, fpath,
		    strerror(errno));

	snprintf(msg, sizeof(msg),
	    "Directory %s added to the repository", rcsdir);

	if (tag != NULL) {
		strlcat(msg, "\n--> Using per-directory sticky tag ",
		    sizeof(msg));
		strlcat(msg, tag, sizeof(msg));
	}
	if (date != NULL) {
		strlcat(msg, "\n--> Using per-directory sticky date ",
		    sizeof(msg));
		strlcat(msg, date, sizeof(msg));
	}
	strlcat(msg, "\n", sizeof(msg));

	if (cvs_noexec == 0) {
		if (mkdir(rcsdir, 0777) == -1)
			fatal("cvs_add_directory: mkdir `%s': %s",
			    rcsdir, strerror(errno));
	}

	/* create CVS/ admin files */
	if (cvs_noexec == 0)
		if (cvs_mkadmin(fpath, root->cr_str, repo, tag, date, nb) == -1)
			return (CVS_EX_FILE);

	/* XXX Build the Entries line. */
	if (strlcpy(entry, "D/", sizeof(entry)) >= sizeof(entry) ||
	    strlcat(entry, fpath, sizeof(entry)) >= sizeof(entry) ||
	    strlcat(entry, "////", sizeof(entry)) >= sizeof(entry))
		fatal("cvs_add_directory: path truncation");

	if ((ent = cvs_ent_parse(entry)) == NULL)
		fatal("cvs_add_directory: cvs_ent_parse failed");

	if (cvs_ent_add(entf, ent) < 0)
		fatal("cvs_add_directory: cvs_ent_parse failed");

	cvs_printf("%s", msg);

	return (0);
}

static int
cvs_add_build_entry(CVSFILE *cf)
{
	char entry[CVS_ENT_MAXLINELEN], path[MAXPATHLEN];
	FILE *fp;
	CVSENTRIES *entf;
	struct cvs_ent *ent;

	entf = (CVSENTRIES *)cf->cf_entry;

	if (cvs_noexec == 1)
		return (0);

	/* Build the path to the <file>,t file. */
	if (strlcpy(path, CVS_PATH_CVSDIR, sizeof(path)) >= sizeof(path) ||
	    strlcat(path, "/", sizeof(path)) >= sizeof(path) ||
	    strlcat(path, cf->cf_name, sizeof(path)) >= sizeof(path) ||
	    strlcat(path, CVS_DESCR_FILE_EXT, sizeof(path)) >= sizeof(path))
		fatal("cvs_add_build_entry: path truncation");

	if ((fp = fopen(path, "w+")) == NULL)
		fatal("cvs_add_build_entry: fopen `%s': %s", path,
		    strerror(errno));

	if (cvs_msg != NULL) {
		if (fputs(cvs_msg, fp) == EOF)
			fatal("cvs_add_build_entry: fputs `%s': %s", path,
			    strerror(errno));
	}
	(void)fclose(fp);

	/* XXX Build the Entries line. */
	if (strlcpy(entry, "/", sizeof(entry)) >= sizeof(entry) ||
	    strlcat(entry, cf->cf_name, sizeof(entry)) >= sizeof(entry) ||
	    strlcat(entry, "/0/Initial ", sizeof(entry)) >= sizeof(entry) ||
	    strlcat(entry, cf->cf_name, sizeof(entry)) >= sizeof(entry) ||
	    strlcat(entry, "/", sizeof(entry)) >= sizeof(entry) ||
	    strlcat(entry, kbuf, sizeof(entry)) >= sizeof(entry) ||
	    strlcat(entry, "/", sizeof(entry)) >= sizeof(entry)) {
		(void)cvs_unlink(path);
		fatal("cvs_add_build_entry: path truncation");
	}

	if ((ent = cvs_ent_parse(entry)) == NULL) {
		(void)cvs_unlink(path);
		fatal("cvs_add_build_entry: cvs_ent_parse failed");
	}

	if (cvs_ent_add(entf, ent) < 0) {
		(void)cvs_unlink(path);
		fatal("cvs_add_build_entry: cvs_ent_add failed");
	}

	return (0);
}
