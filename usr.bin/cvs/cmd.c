/*	$OpenBSD: cmd.c,v 1.5 2005/03/31 17:18:24 joris Exp $	*/
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sysexits.h>

#include "cvs.h"
#include "log.h"
#include "rcs.h"
#include "proto.h"

/*
 * start the execution of a command.
 */
int
cvs_startcmd(struct cvs_cmd *cmd, int argc, char **argv)
{
	int i;
	int ret;
	struct cvsroot *root;
	struct cvs_cmd_info *c = cmd->cmd_info;

	/* if the command requested is the server one, just call the
	 * cvs_server() function to handle it, and return after it.
	 */
	if (cmd->cmd_op == CVS_OP_SERVER) {
		ret = cvs_server(argc, argv);
		return (ret);
	}

	if (c->cmd_options != NULL) {
		if ((ret = c->cmd_options(cmd->cmd_opts, argc, argv, &i)))
			return (ret);

		argc -= i;
		argv += i;
	}

	if ((c->cmd_flags & CVS_CMD_ALLOWSPEC) && argc != 0)
		cvs_files = cvs_file_getspec(argv, argc, 0);
	else
		cvs_files = cvs_file_get(".", c->file_flags);

	if (cvs_files == NULL)
		return (EX_DATAERR);

	if ((c->cmd_helper != NULL) && ((ret = c->cmd_helper())))
		return (ret);

	root = CVS_DIR_ROOT(cvs_files);
	if (root == NULL && (root = cvsroot_get(".")) == NULL) {
		cvs_log(LP_ERR,
		    "No CVSROOT specified! Please use the `-d' option");
		cvs_log(LP_ERR,
		    "or set the CVSROOT enviroment variable.");
		return (EX_USAGE);
	}

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (cvs_connect(root) < 0)
			return (EX_PROTOCOL);

		if (c->cmd_flags & CVS_CMD_SENDARGS1) {
			for (i = 0; i < argc; i++) {
				if (cvs_sendarg(root, argv[i], 0) < 0)
					return (EX_PROTOCOL);
			}
		}

		if (c->cmd_sendflags != NULL) {
			if ((ret = c->cmd_sendflags(root)))
				return (ret);
		}

		if (c->cmd_flags & CVS_CMD_NEEDLOG) {
			if (cvs_logmsg_send(root, cvs_msg) < 0)
				return (EX_PROTOCOL);
		}
	}

	if (c->cmd_examine != NULL)
		cvs_file_examine(cvs_files, c->cmd_examine, NULL);

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (c->cmd_flags & CVS_CMD_SENDDIR) {
			if (cvs_senddir(root, cvs_files) < 0)
				return (EX_PROTOCOL);
		}

		if (c->cmd_flags & CVS_CMD_SENDARGS2) {
			for (i = 0; i < argc; i++) {
				if (cvs_sendarg(root, argv[i], 0) < 0)
					return (EX_PROTOCOL);
			}
		}

		if (cvs_sendreq(root, c->cmd_req,
		    (cmd->cmd_op == CVS_OP_INIT) ? root->cr_dir : NULL) < 0)
			return (EX_PROTOCOL);
	}

	return (0);
}
