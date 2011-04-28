/*	$OpenBSD: mpathvar.h,v 1.2 2011/04/28 10:43:36 dlg Exp $ */

/*
 * Copyright (c) 2010 David Gwynne <dlg@openbsd.org>
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

#ifndef _SYS_SCSI_MPATH_H_
#define _SYS_SCSI_MPATH_H_

struct mpath_dev;
struct mpath_group;

struct mpath_ops {
	char	op_name[16];
	int	(*op_checksense)(struct scsi_xfer *);
	int	(*op_online)(struct scsi_link *);
	int	(*op_offline)(struct scsi_link *);
};

struct mpath_path {
	/* the path driver must set these */
	struct scsi_xshandler	 p_xsh;
	struct scsi_link	*p_link;
	struct mpath_ops	*p_ops;
	int			 p_gid;

	/* the follwoing are private to mpath.c */
	TAILQ_ENTRY(mpath_path)	 p_entry;
	struct mpath_dev	*p_dev;
	int			 p_state;
};

int			 mpath_path_probe(struct scsi_link *);
int			 mpath_path_attach(struct mpath_path *);
void			 mpath_path_state(struct mpath_path *, int);
int			 mpath_path_detach(struct mpath_path *);

void			 mpath_start(struct mpath_path *, struct scsi_xfer *);

struct device		*mpath_bootdv(struct device *);

#endif /* _SYS_SCSI_MPATH_H_ */
