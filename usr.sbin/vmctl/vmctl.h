/*	$OpenBSD: vmctl.h,v 1.5 2015/12/08 08:01:20 reyk Exp $	*/

/*
 * Copyright (c) 2015 Reyk Floeter <reyk@openbsd.org>
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

#include <sys/queue.h>
#include <imsg.h>

#ifndef VMCTL_PARSER_H
#define VMCTL_PARSER_H

#define VMCTL_CU	"/usr/bin/cu"

enum actions {
	NONE,
	CMD_CONSOLE,
	CMD_CREATE,
	CMD_LOAD,
	CMD_RELOAD,
	CMD_START,
	CMD_STATUS,
	CMD_STOP,
};

struct ctl_command;

struct parse_result {
	enum actions		 action;
	uint32_t		 id;
	char			*name;
	char			*path;
	long long		 size;
	int			 nifs;
	size_t			 ndisks;
	char			**disks;
	int			 disable;
	struct ctl_command	*ctl;
};

struct ctl_command {
	const char		*name;
	enum actions		 action;
	int			(*main)(struct parse_result *, int, char *[]);
	const char		*usage;
	int			 has_pledge;
};

struct imsgbuf	*ibuf;

/* main.c */
int	 vmmaction(struct parse_result *);
int	 parse_ifs(struct parse_result *, char *, int);
int	 parse_size(struct parse_result *, char *, long long);
int	 parse_disk(struct parse_result *, char *);
int	 parse_vmid(struct parse_result *, char *, uint32_t);
void	 parse_free(struct parse_result *);
int	 parse(int, char *[]);
__dead void
	 ctl_openconsole(const char *);

/* vmctl.c */
int	 create_imagefile(const char *, long);
int	 start_vm(const char *, int, int, int, char **, char *);
int	 start_vm_complete(struct imsg *, int *, int);
void	 terminate_vm(uint32_t);
int	 terminate_vm_complete(struct imsg *, int *);
void	 get_info_vm(uint32_t, int);
int	 add_info(struct imsg *, int *);
void	 print_vm_info(struct vmop_info_result *, size_t);
__dead void
	 vm_console(struct vmop_info_result *, size_t);

#endif /* VMCTL_PARSER_H */
