/*	$OpenBSD: init.c,v 1.12 2005/03/30 17:43:04 joris Exp $	*/
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

#include <sys/param.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sysexits.h>

#include "cvs.h"
#include "rcs.h"
#include "log.h"
#include "proto.h"


#define CFT_FILE   1
#define CFT_DIR    2


struct cvsroot_file {
	char   *cf_path;   /* path relative to CVS root directory */
	u_int   cf_type;
	mode_t  cf_mode;
} cvsroot_files[] = {
	{ CVS_PATH_ROOT,   CFT_DIR, (S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) },

	{ CVS_PATH_COMMITINFO,  CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_CONFIG,      CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_CVSIGNORE,   CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_CVSWRAPPERS, CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_EDITINFO,    CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_HISTORY,     CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_LOGINFO,     CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_MODULES,     CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_NOTIFY,      CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_RCSINFO,     CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_TAGINFO,     CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
	{ CVS_PATH_VERIFYMSG,   CFT_FILE, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) },
};

int cvs_init_local(struct cvsroot *);

struct cvs_cmd_info cvs_init = {
	NULL,
	NULL, NULL, NULL, NULL,
	0,
	CVS_REQ_INIT,
	0
};

/*
 * cvs_init_local()
 *
 * Local handler for the "cvs init" command.
 * Returns 0 on success, or the appropriate exit status on failure.
 */
int
cvs_init_local(struct cvsroot *root)
{
	int fd;
	u_int i;
	char path[MAXPATHLEN];
	RCSFILE *rfp;

	for (i = 0; i < sizeof(cvsroot_files)/sizeof(cvsroot_files[i]); i++) {
		snprintf(path, sizeof(path), "%s/%s", root->cr_dir,
		    cvsroot_files[i].cf_path);

		if (cvsroot_files[i].cf_type == CFT_DIR) {
			if (mkdir(path, cvsroot_files[i].cf_mode) == -1) {
				cvs_log(LP_ERRNO, "failed to create `%s'",
				    path);
				return (EX_CANTCREAT);
			}
		} else if (cvsroot_files[i].cf_type == CFT_FILE) {
			fd = open(path, O_WRONLY|O_CREAT|O_EXCL,
			    cvsroot_files[i].cf_mode);
			if (fd == -1) {
				cvs_log(LP_ERRNO, "failed to create `%s'",
				    path);
				return (EX_CANTCREAT);
			}

			(void)close(fd);

			strlcat(path, RCS_FILE_EXT, sizeof(path));
			rfp = rcs_open(path, RCS_WRITE|RCS_CREATE, 0640);
			if (rfp == NULL) {
				return (EX_CANTCREAT);
			}

			rcs_close(rfp);
		}
	}

	return (0);
}
