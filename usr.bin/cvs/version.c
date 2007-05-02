/*	$OpenBSD: version.c,v 1.25 2007/05/02 16:26:50 xsa Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * Copyright (c) 2006 Xavier Santolaria <xsa@openbsd.org>
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

#include "cvs.h"
#include "remote.h"

struct cvs_cmd cvs_cmd_version = {
	CVS_OP_VERSION, 0, "version",
	{ "ve", "ver" },
	"Show current CVS version(s)",
	"",
	"",
	NULL,
	cvs_version
};

int
cvs_version(int argc, char **argv)
{
	if (argc > 1)
		fatal("version does not take any extra arguments");

	if (current_cvsroot != NULL &&
	    current_cvsroot->cr_method != CVS_METHOD_LOCAL)
		cvs_printf("Client: ");

	cvs_printf("%s\n", CVS_VERSION);

	if (current_cvsroot != NULL &&
	    current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();
		cvs_client_send_request("version");
		/* XXX: better way to handle server response? */
		cvs_printf("Server: ");
		cvs_client_get_responses();
	}

	return (0);
}
