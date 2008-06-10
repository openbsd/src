/*	$OpenBSD: trigger.h,v 1.2 2008/06/10 05:29:14 xsa Exp $	*/
/*
 * Copyright (c) 2008 Tobias Stoeckmann <tobias@openbsd.org>
 * Copyright (c) 2008 Jonathan Armani <dbd@asystant.net>
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

#define CVS_TRIGGER_COMMITINFO		1
#define CVS_TRIGGER_LOGINFO		2
#define CVS_TRIGGER_VERIFYMSG		3
#define CVS_TRIGGER_RCSINFO		4
#define CVS_TRIGGER_EDITINFO		5
#define CVS_TRIGGER_TAGINFO		6

struct trigger_line {
	char *line;
	TAILQ_ENTRY(trigger_line) flist;
};

TAILQ_HEAD(trigger_list, trigger_line);

struct file_info {
	char	*file_path;
	char	*file_wd;
	char	*crevstr;
	char	*nrevstr;
	char	*tag_new;
	char	*tag_old;
	char	*tag_op;
	char	 tag_type;
	TAILQ_ENTRY(file_info) flist;
};

TAILQ_HEAD(file_info_list, file_info);

int			 cvs_trigger_handle(int, char *, char *,
			     struct trigger_list *, struct file_info_list *);
struct trigger_list	*cvs_trigger_getlines(char *, char *);
void			 cvs_trigger_freelist(struct trigger_list *);
void			 cvs_trigger_freeinfo(struct file_info_list *);
void			 cvs_trigger_loginfo_header(BUF *, char *);
