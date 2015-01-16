/*	$OpenBSD: cmd.c,v 1.69 2015/01/16 06:40:07 deraadt Exp $	*/
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
#include <sys/types.h>
#include <sys/dirent.h>

#include <string.h>

#include "cvs.h"

struct cvs_cmd *cvs_cdt[] = {
	&cvs_cmd_add,
	&cvs_cmd_admin,
	&cvs_cmd_annotate,
	&cvs_cmd_commit,
	&cvs_cmd_checkout,
	&cvs_cmd_diff,
	&cvs_cmd_export,
	&cvs_cmd_history,
	&cvs_cmd_import,
	&cvs_cmd_init,
	&cvs_cmd_log,
	&cvs_cmd_rannotate,
	&cvs_cmd_rdiff,
	&cvs_cmd_release,
	&cvs_cmd_remove,
	&cvs_cmd_rlog,
	&cvs_cmd_rtag,
	&cvs_cmd_server,
	&cvs_cmd_status,
	&cvs_cmd_tag,
	&cvs_cmd_update,
	&cvs_cmd_version,
#if 0
	&cvs_cmd_edit,
	&cvs_cmd_editors,
	&cvs_cmd_unedit,
	&cvs_cmd_watch,
	&cvs_cmd_watchers,
#endif
	NULL
};

struct cvs_cmd *
cvs_findcmd(const char *cmd)
{
	int i, j;
	struct cvs_cmd *p;

	p = NULL;
	for (i = 0; (cvs_cdt[i] != NULL) && (p == NULL); i++) {
		if (strcmp(cmd, cvs_cdt[i]->cmd_name) == 0)
			p = cvs_cdt[i];
		else {
			for (j = 0; j < CVS_CMD_MAXALIAS; j++) {
				if (strcmp(cmd,
				    cvs_cdt[i]->cmd_alias[j]) == 0) {
					p = cvs_cdt[i];
					break;
				}
			}
		}
	}

	return (p);
}
