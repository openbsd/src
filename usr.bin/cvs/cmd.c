/*	$OpenBSD: cmd.c,v 1.38 2005/12/03 01:02:08 joris Exp $	*/
/*
 * Copyright (c) 2005 Joris Vink <joris@openbsd.org>
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

#include <sys/queue.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"

extern char *cvs_rootstr;

/*
 * Command dispatch table
 * ----------------------
 *
 * The synopsis field should only contain the list of arguments that the
 * command supports, without the actual command's name.
 *
 * Command handlers are expected to return 0 if no error occurred, or one of
 * the CVS_EX_* error codes in case of an error.  In case the error
 * returned is 1, the command's usage string is printed to standard
 * error before returning.
 */
struct cvs_cmd *cvs_cdt[] = {
	&cvs_cmd_add,
	&cvs_cmd_admin,
	&cvs_cmd_annotate,
	&cvs_cmd_checkout,
	&cvs_cmd_commit,
	&cvs_cmd_diff,
	&cvs_cmd_edit,
	&cvs_cmd_editors,
	&cvs_cmd_export,
	&cvs_cmd_history,
	&cvs_cmd_import,
	&cvs_cmd_init,
#if defined(HAVE_KERBEROS)
	&cvs_cmd_kserver,
#endif
	&cvs_cmd_log,
#if 0
	&cvs_cmd_login,
	&cvs_cmd_logout,
#endif
	&cvs_cmd_rdiff,
	&cvs_cmd_release,
	&cvs_cmd_remove,
	&cvs_cmd_rlog,
	&cvs_cmd_rtag,
	&cvs_cmd_server,
	&cvs_cmd_status,
	&cvs_cmd_tag,
	&cvs_cmd_unedit,
	&cvs_cmd_update,
	&cvs_cmd_version,
	&cvs_cmd_watch,
	&cvs_cmd_watchers,
	NULL
};

#define MISSING_CVS_DIR		0x01
#define MISSING_CVS_ENTRIES	0x02
#define MISSING_CVS_REPO	0x04

/*
 * cvs_findcmd()
 *
 * Find the entry in the command dispatch table whose name or one of its
 * aliases matches <cmd>.
 * Returns a pointer to the command entry on success, NULL on failure.
 */
struct cvs_cmd *
cvs_findcmd(const char *cmd)
{
	int i, j;
	struct cvs_cmd *cmdp;

	cmdp = NULL;

	for (i = 0; (cvs_cdt[i] != NULL) && (cmdp == NULL); i++) {
		if (strcmp(cmd, cvs_cdt[i]->cmd_name) == 0)
			cmdp = cvs_cdt[i];
		else {
			for (j = 0; j < CVS_CMD_MAXALIAS; j++) {
				if (strcmp(cmd,
				    cvs_cdt[i]->cmd_alias[j]) == 0) {
					cmdp = cvs_cdt[i];
					break;
				}
			}
		}
	}

	return (cmdp);
}

struct cvs_cmd *
cvs_findcmdbyreq(int reqid)
{
	int i;
	struct cvs_cmd *cmdp;

	cmdp = NULL;
	for (i = 0; cvs_cdt[i] != NULL; i++)
		if (cvs_cdt[i]->cmd_req == reqid) {
			cmdp = cvs_cdt[i];
			break;
		}

	return (cmdp);
}


/*
 * start the execution of a command.
 */
int
cvs_startcmd(struct cvs_cmd *cmd, int argc, char **argv)
{
	int i, ret, error;
	struct cvsroot *root;
	int (*ex_hdlr)(CVSFILE *, void *);
	CVSFILE *cf;
	struct stat st;

	/* if the command requested is the server one, just call the
	 * cvs_server() function to handle it, and return after it.
	 */
	if (cmd->cmd_op == CVS_OP_SERVER)
		return cvs_server(argc, argv);

	if ((root = cvsroot_get(".")) == NULL)
		return (CVS_EX_BADROOT);

	i = 1;
	if (cmd->cmd_init != NULL) {
		if ((ret = (*cmd->cmd_init)(cmd, argc, argv, &i)) != 0)
			return (ret);
	}

	argc -= i;
	argv += i;

	/*
	 * Check if we have the administrative files present, if we are
	 * missing one, we will error out because we cannot continue.
	 *
	 * We are not checking for CVS/Root since we fetched the root
	 * above via cvsroot_get().
	 *
	 * checkout, export, import, init and release do not depend on these files.
	 */
	error = 0;
	if ((cmd->cmd_op != CVS_OP_CHECKOUT) &&
	    (cmd->cmd_op != CVS_OP_EXPORT) &&
	    (cmd->cmd_op != CVS_OP_IMPORT) &&
	    (cmd->cmd_op != CVS_OP_INIT) &&
	    (cmd->cmd_op != CVS_OP_RELEASE) &&
	    (cmd->cmd_op != CVS_OP_VERSION)) {
		/* check for the CVS directory */
		ret = stat(CVS_PATH_CVSDIR, &st);
		if (((ret == -1) && (errno == ENOENT)) || ((ret != -1) &&
		    !(S_ISDIR(st.st_mode))))
			error |= MISSING_CVS_DIR;

		/* check if the CVS/Entries file exists */
		ret = stat(CVS_PATH_ENTRIES, &st);
		if (((ret == -1) && (errno == ENOENT)) || ((ret != -1) &&
		    !(S_ISREG(st.st_mode))))
			error |= MISSING_CVS_ENTRIES;

		/* check if the CVS/Repository file exists */
		ret = stat(CVS_PATH_REPOSITORY, &st);
		if (((ret == -1) && (errno == ENOENT)) || ((ret != -1) &&
		    !(S_ISREG(st.st_mode))))
			error |= MISSING_CVS_REPO;
	}

	if (error > 0) {
		if (error & MISSING_CVS_DIR) {
			cvs_log(LP_ABORT, "missing '%s' directory",
			    CVS_PATH_CVSDIR);
			return (CVS_EX_FILE);
		}

		if (error & MISSING_CVS_ENTRIES)
			cvs_log(LP_ABORT, "missing '%s' file",
			    CVS_PATH_ENTRIES);
		if (error & MISSING_CVS_REPO)
			cvs_log(LP_ABORT, "missing '%s' file",
			    CVS_PATH_REPOSITORY);
		return (CVS_EX_FILE);
	}

	if (!(cmd->cmd_flags & CVS_CMD_ALLOWSPEC) && (argc > 0))
		return (CVS_EX_USAGE);

	/*
	 * This allows us to correctly fill in the repository
	 * string for CVSFILE's fetched inside the repository itself.
	 */
	if (cvs_cmdop == CVS_OP_SERVER) {
		cvs_rootstr = strdup(root->cr_str);
		if (cvs_rootstr == NULL)
			return (CVS_EX_DATA);
	}

	cvs_log(LP_TRACE, "cvs_startcmd() CVSROOT=%s", root->cr_str);

	if ((root->cr_method != CVS_METHOD_LOCAL) && (cvs_connect(root) < 0))
		return (CVS_EX_PROTO);

	if (cmd->cmd_pre_exec != NULL) {
		if ((ret = cmd->cmd_pre_exec(root)) != 0)
			return (ret);
	}

	if (root->cr_method == CVS_METHOD_LOCAL)
		ex_hdlr = cmd->cmd_exec_local;
	else
		ex_hdlr = cmd->cmd_exec_remote;

	if (argc > 0) {
		ret = cvs_file_getspec(argv, argc, cmd->file_flags,
		    ex_hdlr, NULL, NULL);
	} else {
		ret = cvs_file_get(".", cmd->file_flags,
		    ex_hdlr, NULL, NULL);
	}

	if (ret != CVS_EX_OK)
		return (cvs_error);

	if (cmd->cmd_post_exec != NULL) {
		if ((ret = cmd->cmd_post_exec(root)) != 0)
			return (ret);
	}

	if (root->cr_method != CVS_METHOD_LOCAL) {
		/*
		 * If we have to send the directory the command
		 * has been issued in, obtain it.
		 */
		if (cmd->cmd_flags & CVS_CMD_SENDDIR) {
			cf = cvs_file_loadinfo(".", CF_NOFILES, NULL, NULL, 1);
			if (cf == NULL)
				return (CVS_EX_DATA);
			if (cvs_senddir(root, cf) < 0) {
				cvs_file_free(cf);
				return (CVS_EX_PROTO);
			}
			cvs_file_free(cf);
		}

		if (cmd->cmd_flags & CVS_CMD_SENDARGS2) {
			for (i = 0; i < argc; i++) {
				if (cvs_sendarg(root, argv[i], 0) < 0)
					return (CVS_EX_PROTO);
			}
		}

		if (cmd->cmd_req != CVS_REQ_NONE &&
		    cvs_sendreq(root, cmd->cmd_req,
		    (cmd->cmd_op == CVS_OP_INIT) ? root->cr_dir : NULL) < 0)
			return (CVS_EX_PROTO);
	}

	if (cmd->cmd_cleanup != NULL)
		(*cmd->cmd_cleanup)();

#if 0
	if (cvs_cmdop != CVS_OP_SERVER && cmd->cmd_flags & CVS_CMD_PRUNEDIRS)
		cvs_file_prune(fpath);
#endif

	if (root->cr_method != CVS_METHOD_LOCAL)
		cvs_disconnect(root);

	return (0);
}
