/*	$OpenBSD: cmd.c,v 1.25 2005/06/13 13:02:18 xsa Exp $	*/
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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"


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
#if 0
	&cvs_cmd_edit,
	&cvs_cmd_editors,
	&cvs_cmd_export,
#endif
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
#if 0
	&cvs_cmd_unedit,
#endif
	&cvs_cmd_update,
	&cvs_cmd_version,
#if 0
	&cvs_cmd_watch,
	&cvs_cmd_watchers,
#endif
	NULL
};



/*
 * cvs_findcmd()
 *
 * Find the entry in the command dispatch table whose name or one of its
 * aliases matches <cmd>.
 * Returns a pointer to the command entry on success, NULL on failure.
 */
struct cvs_cmd*
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

struct cvs_cmd*
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
	int i;
	int ret;
	struct cvsroot *root;
	int (*ex_hdlr)(CVSFILE *, void *);

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

	if (!(cmd->cmd_flags & CVS_CMD_ALLOWSPEC) && (argc > 0))
		return (CVS_EX_USAGE);

	if ((root->cr_method != CVS_METHOD_LOCAL) && (cvs_connect(root) < 0))
		return (CVS_EX_PROTO);

	cvs_log(LP_TRACE, "cvs_startcmd() CVSROOT=%s", root->cr_str);

	if (cmd->cmd_pre_exec != NULL) {
		if ((ret = cmd->cmd_pre_exec(root)) != 0)
			return (ret);
	}

	if (root->cr_method == CVS_METHOD_LOCAL)
		ex_hdlr = cmd->cmd_exec_local;
	else
		ex_hdlr = cmd->cmd_exec_remote;

	if (argc > 0) {
		cvs_files = cvs_file_getspec(argv, argc, cmd->file_flags,
		    ex_hdlr, NULL);
	} else {
		cvs_files = cvs_file_get(".", cmd->file_flags,
		    ex_hdlr, NULL);
	}

	if (cvs_files == NULL)
		return (CVS_EX_DATA);

	if (cmd->cmd_post_exec != NULL) {
		if ((ret = cmd->cmd_post_exec(root)) != 0)
			return (ret);
	}

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (cmd->cmd_flags & CVS_CMD_SENDDIR) {
			if (cvs_senddir(root, cvs_files) < 0)
				return (CVS_EX_PROTO);
		}

		if (cmd->cmd_flags & CVS_CMD_SENDARGS2) {
			for (i = 0; i < argc; i++) {
				if (cvs_sendarg(root, argv[i], 0) < 0)
					return (CVS_EX_PROTO);
			}
		}

		if (cmd->cmd_req != CVS_REQ_NONE && cvs_sendreq(root, cmd->cmd_req,
		    (cmd->cmd_op == CVS_OP_INIT) ? root->cr_dir : NULL) < 0)
			return (CVS_EX_PROTO);
	}

	if (cmd->cmd_cleanup != NULL)
		(*cmd->cmd_cleanup)();

	if (root->cr_method != CVS_METHOD_LOCAL)
		cvs_disconnect(root);

	return (0);
}
