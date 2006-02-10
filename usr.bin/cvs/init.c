/*	$OpenBSD: init.c,v 1.22 2006/02/10 10:15:48 xsa Exp $	*/
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
#include "proto.h"


#define CFT_FILE	1
#define CFT_DIR		2


struct cvsroot_file {
	char	*cf_path;	/* path relative to CVS root directory */
	u_int	 cf_type;
	mode_t	 cf_mode;
} cvsroot_files[] = {
	{ CVS_PATH_ROOT,   CFT_DIR, (S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) },
	{ CVS_PATH_EMPTYDIR, CFT_DIR, (S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) },
	{ CVS_PATH_COMMITINFO,  CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_CONFIG,      CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_CVSIGNORE,   CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_CVSWRAPPERS, CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_EDITINFO,    CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_HISTORY,     CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_LOGINFO,     CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_MODULES,     CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_NOTIFY_R,    CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_RCSINFO,     CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_TAGINFO,     CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_VERIFYMSG,   CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
};

static int	cvs_init_pre_exec(struct cvsroot *);
static void	cvs_init_create_files(struct cvsroot *);

struct cvs_cmd cvs_cmd_init = {
	CVS_OP_INIT, CVS_REQ_INIT, "init",
	{ },
	"Create a CVS repository if it doesn't exist",
	"",
	"",
	NULL,
	0,
	NULL,
	cvs_init_pre_exec,
	NULL,
	NULL,
	NULL,
	NULL,
	0
};

/*
 * cvs_init_pre_exec()
 *
 * Local/remote handler for the "cvs init" command.
 * Returns 0 on success, -1 on failure.
 */
static int
cvs_init_pre_exec(struct cvsroot *root)
{
	if (root->cr_method == CVS_METHOD_LOCAL)
		cvs_init_create_files(root);

	return (0);
}

/*
 * cvs_init_create_files
 *
 * Create all required files for the "cvs init" command.
 * Used by the local handlers.
 * Returns 0 on success, -1 on failure.
 *
 */
static void
cvs_init_create_files(struct cvsroot *root)
{
	size_t len;
	int fd;
	u_int i;
	char path[MAXPATHLEN];
	RCSFILE *rfp;
	struct stat st;

	/* Create repository root directory if it does not already exist */
	if (mkdir(root->cr_dir, 0777) == -1) {
		if (!(errno == EEXIST || (errno == EACCES &&
		    (stat(root->cr_dir, &st) == 0) && S_ISDIR(st.st_mode)))) {
			fatal("cvs_init_create_files: mkdir: %s: %s",
			    root->cr_dir, strerror(errno));
		}
	}

	/* Create the repository administrative files */
	for (i = 0; i < sizeof(cvsroot_files)/sizeof(cvsroot_files[i]); i++) {
		len = cvs_path_cat(root->cr_dir, cvsroot_files[i].cf_path,
		    path, sizeof(path));
		if (len >= sizeof(path))
			fatal("cvs_init_create_files: path truncation");

		if (cvsroot_files[i].cf_type == CFT_DIR) {
			if (mkdir(path, cvsroot_files[i].cf_mode) == -1) {
				if (!(errno == EEXIST || (errno == EACCES &&
				    (stat(path, &st) == 0) &&
				    S_ISDIR(st.st_mode)))) {
					fatal("cvs_init_create_files: mkdir: "
					    "%s: %s", path, strerror(errno));
				}
			}
		} else if (cvsroot_files[i].cf_type == CFT_FILE) {
			fd = open(path, O_WRONLY|O_CREAT|O_EXCL,
			    cvsroot_files[i].cf_mode);
			if (fd == -1)
				fatal("cvs_init_create_file: open failed: %s",
				    strerror(errno));

			(void)close(fd);

			strlcat(path, RCS_FILE_EXT, sizeof(path));
			rfp = rcs_open(path, RCS_WRITE|RCS_CREATE, 0640);
			if (rfp == NULL)
				return;

			rcs_close(rfp);
		}
	}
}
