/*	$OpenBSD: watch.c,v 1.21 2008/06/23 20:51:08 ragge Exp $	*/
/*
 * Copyright (c) 2005-2007 Xavier Santolaria <xsa@openbsd.org>
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

#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "remote.h"

#define W_COMMIT	0x01
#define W_EDIT		0x02
#define W_UNEDIT	0x04
#define W_ADD		0x08
#define W_REMOVE	0x10
#define W_ON		0x20
#define W_OFF		0x40
#define W_ALL		(W_EDIT|W_COMMIT|W_UNEDIT)

static void	cvs_watch_local(struct cvs_file *);
static void	cvs_watchers_local(struct cvs_file *);

static int	watch_req = 0;
static int	watch_aflags = 0;

struct cvs_cmd cvs_cmd_watch = {
	CVS_OP_WATCH, CVS_USE_WDIR, "watch",
	{ { 0 }, { 0 } },
	"Set watches",
	"on | off | add | remove [-lR] [-a action] [file ...]",
	"a:lR",
	NULL,
	cvs_watch
};

struct cvs_cmd cvs_cmd_watchers = {
	CVS_OP_WATCHERS, CVS_USE_WDIR, "watchers",
	{ { 0 }, { 0 } },
	"See who is watching a file",
	"[-lR] [file ...]",
	"lR",
	NULL,
	cvs_watchers
};

int
cvs_watch(int argc, char **argv)
{
	int ch, flags;
	struct cvs_recursion cr;

	if (argc < 2)
		fatal("%s", cvs_cmd_watch.cmd_synopsis);

	if (strcmp(argv[1], "on") == 0)
		watch_req |= W_ON;
	else if (strcmp(argv[1], "off") == 0)
		watch_req |= W_OFF;
	else if (strcmp(argv[1], "add") == 0)
		watch_req |= W_ADD;
	else if (strcmp(argv[1], "remove") == 0)
		watch_req |= W_REMOVE;
	else
		fatal("%s", cvs_cmd_watch.cmd_synopsis);

	--argc;
	++argv;

	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmd_watch.cmd_opts)) != -1) {
		switch (ch) {
		case 'a':
			if (!(watch_req & (W_ADD|W_REMOVE)))
				fatal("%s", cvs_cmd_watch.cmd_synopsis);

			if (strcmp(optarg, "edit") == 0)
				watch_aflags |= W_EDIT;
			else if (strcmp(optarg, "unedit") == 0)
				watch_aflags |= W_UNEDIT;
			else if (strcmp(optarg, "commit") == 0)
				watch_aflags |= W_COMMIT;
			else if (strcmp(optarg, "all") == 0)
				watch_aflags |= W_ALL;
			else if (strcmp(optarg, "none") == 0)
				watch_aflags &= ~W_ALL;
			else
				fatal("%s", cvs_cmd_watch.cmd_synopsis);
			break;
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'R':
			flags |= CR_RECURSE_DIRS;
			break;
		default:
			fatal("%s", cvs_cmd_watch.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (watch_aflags == 0)
		watch_aflags |= W_ALL;

	cr.enterdir = NULL;
	cr.leavedir = NULL;

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();
		cr.fileproc = cvs_client_sendfile;

		if (watch_req & (W_ADD|W_REMOVE)) {
			if (watch_aflags & W_EDIT)
				cvs_client_send_request("Argument -a edit");

			if (watch_aflags & W_UNEDIT)
				cvs_client_send_request("Argument -a unedit");

			if (watch_aflags & W_COMMIT)
				cvs_client_send_request("Argument -a commit");

			if (!(watch_aflags & W_ALL))
				cvs_client_send_request("Argument -a none");
		}

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");
	} else {
		cr.fileproc = cvs_watch_local;
	}

	cr.flags = flags;

	cvs_file_run(argc, argv, &cr);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");

		if (watch_req & (W_ADD|W_REMOVE))
			cvs_client_send_request("watch-%s",
			    (watch_req & W_ADD) ? "add" : "remove");
		else
			cvs_client_send_request("watch-%s",
			    (watch_req & W_ON) ? "on" : "off");

		cvs_client_get_responses();
	}

	return (0);
}

int
cvs_watchers(int argc, char **argv)
{
	int ch;
	int flags;
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmd_watchers.cmd_opts)) != -1) {
		switch (ch) {
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'R':
			flags |= CR_RECURSE_DIRS;
			break;
		default:
			fatal("%s", cvs_cmd_watchers.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		fatal("%s", cvs_cmd_watchers.cmd_synopsis);

	cr.enterdir = NULL;
	cr.leavedir = NULL;

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();
		cr.fileproc = cvs_client_sendfile;

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");
	} else {
		cr.fileproc = cvs_watchers_local;
	}

	cr.flags = flags;

	cvs_file_run(argc, argv, &cr);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");
		cvs_client_send_request("watchers");
		cvs_client_get_responses();
	}

	return (0);
}

static void
cvs_watch_local(struct cvs_file *cf)
{
}

static void
cvs_watchers_local(struct cvs_file *cf)
{
}
