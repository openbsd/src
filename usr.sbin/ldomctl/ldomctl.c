/*	$OpenBSD: ldomctl.c,v 1.21 2018/09/15 13:20:16 kettenis Exp $	*/

/*
 * Copyright (c) 2012 Mark Kettenis
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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ds.h"
#include "hvctl.h"
#include "mdstore.h"
#include "mdesc.h"
#include "util.h"
#include "ldomctl.h"

extern struct ds_service pri_service;

struct command {
	const char *cmd_name;
	void (*cmd_func)(int, char **);
};

__dead void usage(void);

struct guest_head guest_list;

uint64_t find_guest(const char *);

void fetch_pri(void);

void download(int argc, char **argv);
void dump(int argc, char **argv);
void list(int argc, char **argv);
void xselect(int argc, char **argv);
void delete(int argc, char **argv);
void guest_start(int argc, char **argv);
void guest_stop(int argc, char **argv);
void guest_panic(int argc, char **argv);
void guest_status(int argc, char **argv);
void init_system(int argc, char **argv);

struct command commands[] = {
	{ "download",	download },
	{ "dump",	dump },
	{ "list",	list },
	{ "select",	xselect },
	{ "delete",	delete },
	{ "start",	guest_start },
	{ "stop",	guest_stop },
	{ "panic",	guest_panic },
	{ "status",	guest_status },
	{ "init-system", init_system },
	{ NULL,		NULL }
};

void hv_open(void);
void hv_close(void);
void hv_read(uint64_t, void *, size_t);
void hv_write(uint64_t, void *, size_t);

int hvctl_seq = 1;
int hvctl_fd;

void *hvmd_buf;
size_t hvmd_len;
struct md *hvmd;
uint64_t hv_mdpa;
uint64_t hv_membase;
uint64_t hv_memsize;

extern void *pri_buf;
extern size_t pri_len;

int
main(int argc, char **argv)
{
	struct command *cmdp;
	struct hvctl_msg msg;
	ssize_t nbytes;
	struct md_header hdr;
	struct md_node *node;
	struct md_prop *prop;

	if (argc < 2)
		usage();

	/* Skip program name. */
	argv++;
	argc--;

	for (cmdp = commands; cmdp->cmd_name != NULL; cmdp++)
		if (strcmp(argv[0], cmdp->cmd_name) == 0)
			break;
	if (cmdp->cmd_name == NULL)
		usage();

	hv_open();

	/*
	 * Request config.
	 */
	bzero(&msg, sizeof(msg));
	msg.hdr.op = HVCTL_OP_GET_HVCONFIG;
	msg.hdr.seq = hvctl_seq++;
	nbytes = write(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "write");

	bzero(&msg, sizeof(msg));
	nbytes = read(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "read");

	hv_membase = msg.msg.hvcnf.hv_membase;
	hv_memsize = msg.msg.hvcnf.hv_memsize;

	hv_mdpa = msg.msg.hvcnf.hvmdp;
	hv_read(hv_mdpa, &hdr, sizeof(hdr));
	hvmd_len = sizeof(hdr) + hdr.node_blk_sz + hdr.name_blk_sz +
	    hdr.data_blk_sz;
	hvmd_buf = xmalloc(hvmd_len);
	hv_read(hv_mdpa, hvmd_buf, hvmd_len);

	hvmd = md_ingest(hvmd_buf, hvmd_len);
	node = md_find_node(hvmd, "guests");
	TAILQ_INIT(&guest_list);
	TAILQ_FOREACH(prop, &node->prop_list, link) {
		if (prop->tag == MD_PROP_ARC &&
		    strcmp(prop->name->str, "fwd") == 0)
			add_guest(prop->d.arc.node);
	}

	(cmdp->cmd_func)(argc, argv);

	exit(EXIT_SUCCESS);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s start|stop|panic domain\n", __progname);
	fprintf(stderr, "       %s status [domain]\n", __progname);
	exit(EXIT_FAILURE);
}

void
add_guest(struct md_node *node)
{
	struct guest *guest;
	struct md_prop *prop;

	guest = xmalloc(sizeof(*guest));

	if (!md_get_prop_str(hvmd, node, "name", &guest->name))
		goto free;
	if (!md_get_prop_val(hvmd, node, "gid", &guest->gid))
		goto free;
	if (!md_get_prop_val(hvmd, node, "mdpa", &guest->mdpa))
		goto free;

	guest->num_cpus = 0;
	TAILQ_FOREACH(prop, &node->prop_list, link) {
		if (prop->tag == MD_PROP_ARC &&
		    strcmp(prop->name->str, "fwd") == 0) {
			if (strcmp(prop->d.arc.node->name->str, "cpu") == 0)
				guest->num_cpus++;
		}
	}

	TAILQ_INSERT_TAIL(&guest_list, guest, link);
	return;

free:
	free(guest);
}

uint64_t
find_guest(const char *name)
{
	struct guest *guest;

	TAILQ_FOREACH(guest, &guest_list, link) {
		if (strcmp(guest->name, name) == 0)
			return guest->gid;
	}

	errx(EXIT_FAILURE, "unknown guest '%s'", name);
}

void
fetch_pri(void)
{
	struct ds_conn *dc;

	dc = ds_conn_open("/dev/spds", NULL);
	ds_conn_register_service(dc, &pri_service);
	while (pri_buf == NULL)
		ds_conn_handle(dc);
}

void
dump(int argc, char **argv)
{
	struct guest *guest;
	struct md_header hdr;
	void *md_buf;
	size_t md_len;
	char *name;
	FILE *fp;

	if (argc != 1)
		usage();

	fp = fopen("hv.md", "w");
	if (fp == NULL)
		err(1, "fopen");
	fwrite(hvmd_buf, hvmd_len, 1, fp);
	fclose(fp);

	fetch_pri();

	fp = fopen("pri", "w");
	if (fp == NULL)
		err(1, "fopen");
	fwrite(pri_buf, pri_len, 1, fp);
	fclose(fp);

	TAILQ_FOREACH(guest, &guest_list, link) {
		hv_read(guest->mdpa, &hdr, sizeof(hdr));
		md_len = sizeof(hdr) + hdr.node_blk_sz + hdr.name_blk_sz +
		    hdr.data_blk_sz;
		md_buf = xmalloc(md_len);
		hv_read(guest->mdpa, md_buf, md_len);

		if (asprintf(&name, "%s.md", guest->name) == -1)
			err(1, "asprintf");

		fp = fopen(name, "w");
		if (fp == NULL)
			err(1, "fopen");
		fwrite(md_buf, md_len, 1, fp);
		fclose(fp);

		free(name);
		free(md_buf);
	}
}

void
init_system(int argc, char **argv)
{
	if (argc != 2)
		usage();

	build_config(argv[1]);
}

void
list(int argc, char **argv)
{
	struct ds_conn *dc;
	struct mdstore_set *set;

	dc = ds_conn_open("/dev/spds", NULL);
	mdstore_register(dc);
	while (TAILQ_EMPTY(&mdstore_sets))
		ds_conn_handle(dc);

	TAILQ_FOREACH(set, &mdstore_sets, link) {
		printf("%s", set->name);
		if (set->booted_set)
			printf(" [current]");
		else if (set->boot_set)
			printf(" [next]");
		printf("\n");
	}
}

void
xselect(int argc, char **argv)
{
	struct ds_conn *dc;

	if (argc < 2)
		usage();

	dc = ds_conn_open("/dev/spds", NULL);
	mdstore_register(dc);
	while (TAILQ_EMPTY(&mdstore_sets))
		ds_conn_handle(dc);

	mdstore_select(dc, argv[1]);
}

void
delete(int argc, char **argv)
{
	struct ds_conn *dc;

	if (argc < 2)
		usage();

	if (strcmp(argv[1], "factory-default") == 0)
		errx(1, "\"%s\" should not be deleted", argv[1]);

	dc = ds_conn_open("/dev/spds", NULL);
	mdstore_register(dc);
	while (TAILQ_EMPTY(&mdstore_sets))
		ds_conn_handle(dc);

	mdstore_delete(dc, argv[1]);
}

void
download(int argc, char **argv)
{
	struct ds_conn *dc;

	if (argc < 2)
		usage();

	dc = ds_conn_open("/dev/spds", NULL);
	mdstore_register(dc);
	while (TAILQ_EMPTY(&mdstore_sets))
		ds_conn_handle(dc);

	mdstore_download(dc, argv[1]);
}

void
guest_start(int argc, char **argv)
{
	struct hvctl_msg msg;
	ssize_t nbytes;

	if (argc < 2)
		usage();

	/*
	 * Start guest domain.
	 */
	bzero(&msg, sizeof(msg));
	msg.hdr.op = HVCTL_OP_GUEST_START;
	msg.hdr.seq = hvctl_seq++;
	msg.msg.guestop.guestid = find_guest(argv[1]);
	nbytes = write(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "write");

	bzero(&msg, sizeof(msg));
	nbytes = read(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "read");
}

void
guest_stop(int argc, char **argv)
{
	struct hvctl_msg msg;
	ssize_t nbytes;

	if (argc < 2)
		usage();

	/*
	 * Stop guest domain.
	 */
	bzero(&msg, sizeof(msg));
	msg.hdr.op = HVCTL_OP_GUEST_STOP;
	msg.hdr.seq = hvctl_seq++;
	msg.msg.guestop.guestid = find_guest(argv[1]);
	nbytes = write(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "write");

	bzero(&msg, sizeof(msg));
	nbytes = read(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "read");
}

void
guest_panic(int argc, char **argv)
{
	struct hvctl_msg msg;
	ssize_t nbytes;

	if (argc < 2)
		usage();

	/*
	 * Stop guest domain.
	 */
	bzero(&msg, sizeof(msg));
	msg.hdr.op = HVCTL_OP_GUEST_PANIC;
	msg.hdr.seq = hvctl_seq++;
	msg.msg.guestop.guestid = find_guest(argv[1]);
	nbytes = write(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "write");

	bzero(&msg, sizeof(msg));
	nbytes = read(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "read");
}

void
guest_status(int argc, char **argv)
{
	struct hvctl_msg msg;
	ssize_t nbytes;
	struct hvctl_rs_guest_state state;
	struct hvctl_rs_guest_softstate softstate;
	struct hvctl_rs_guest_util util;
	struct guest *guest;
	uint64_t gid = -1;
	uint64_t total_cycles, yielded_cycles;
	double utilisation = 0.0;
	const char *state_str;
	char buf[64];

	if (argc < 1 || argc > 2)
		usage();
	if (argc == 2)
		gid = find_guest(argv[1]);

	TAILQ_FOREACH(guest, &guest_list, link) {
		if (gid != -1 && guest->gid != gid)
			continue;

		/*
		 * Request status.
		 */
		bzero(&msg, sizeof(msg));
		msg.hdr.op = HVCTL_OP_GET_RES_STAT;
		msg.hdr.seq = hvctl_seq++;
		msg.msg.resstat.res = HVCTL_RES_GUEST;
		msg.msg.resstat.resid = guest->gid;
		msg.msg.resstat.infoid = HVCTL_INFO_GUEST_STATE;
		nbytes = write(hvctl_fd, &msg, sizeof(msg));
		if (nbytes != sizeof(msg))
			err(1, "write");

		bzero(&msg, sizeof(msg));
		nbytes = read(hvctl_fd, &msg, sizeof(msg));
		if (nbytes != sizeof(msg))
			err(1, "read");

		memcpy(&state, msg.msg.resstat.data, sizeof(state));
		switch (state.state) {
		case GUEST_STATE_STOPPED:
			state_str = "stopped";
			break;
		case GUEST_STATE_RESETTING:
			state_str = "resetting";
			break;
		case GUEST_STATE_NORMAL:
			state_str = "running";

			bzero(&msg, sizeof(msg));
			msg.hdr.op = HVCTL_OP_GET_RES_STAT;
			msg.hdr.seq = hvctl_seq++;
			msg.msg.resstat.res = HVCTL_RES_GUEST;
			msg.msg.resstat.resid = guest->gid;
			msg.msg.resstat.infoid = HVCTL_INFO_GUEST_SOFT_STATE;
			nbytes = write(hvctl_fd, &msg, sizeof(msg));
			if (nbytes != sizeof(msg))
				err(1, "write");

			bzero(&msg, sizeof(msg));
			nbytes = read(hvctl_fd, &msg, sizeof(msg));
			if (nbytes != sizeof(msg))
				err(1, "read");

			memcpy(&softstate, msg.msg.resstat.data,
			   sizeof(softstate));

			bzero(&msg, sizeof(msg));
			msg.hdr.op = HVCTL_OP_GET_RES_STAT;
			msg.hdr.seq = hvctl_seq++;
			msg.msg.resstat.res = HVCTL_RES_GUEST;
			msg.msg.resstat.resid = guest->gid;
			msg.msg.resstat.infoid = HVCTL_INFO_GUEST_UTILISATION;
			nbytes = write(hvctl_fd, &msg, sizeof(msg));
			if (nbytes != sizeof(msg))
				err(1, "write");

			bzero(&msg, sizeof(msg));
			nbytes = read(hvctl_fd, &msg, sizeof(msg));
			if (nbytes != sizeof(msg))
				err(1, "read");

			memcpy(&util, msg.msg.resstat.data, sizeof(util));

			total_cycles = util.active_delta * guest->num_cpus
			    - util.stopped_cycles;
			yielded_cycles = util.yielded_cycles;
			if (yielded_cycles <= total_cycles)
				utilisation = (100.0 * (total_cycles
				    - yielded_cycles)) / total_cycles;
			else
				utilisation = 0.0;

			break;
		case GUEST_STATE_SUSPENDED:
			state_str = "suspended";
			break;
		case GUEST_STATE_EXITING:
			state_str = "exiting";
			break;
		case GUEST_STATE_UNCONFIGURED:
			state_str = "unconfigured";
			break;
		default:
			snprintf(buf, sizeof(buf), "unknown (%lld)",
			    state.state);
			state_str = buf;
			break;
		}

		if (state.state != GUEST_STATE_NORMAL)
			printf("%-16s  %-16s\n", guest->name, state_str);
		else
			printf("%-16s  %-16s  %-32s  %3.0f%%\n", guest->name,
			       state_str, softstate.soft_state_str,
			       utilisation);
	}
}

void
hv_open(void)
{
	struct hvctl_msg msg;
	ssize_t nbytes;
	uint64_t code;

	hvctl_fd = open("/dev/hvctl", O_RDWR, 0);
	if (hvctl_fd == -1)
		err(1, "cannot open /dev/hvctl");

	/*
	 * Say "Hello".
	 */
	bzero(&msg, sizeof(msg));
	msg.hdr.op = HVCTL_OP_HELLO;
	msg.hdr.seq = hvctl_seq++;
	msg.msg.hello.major = 1;
	nbytes = write(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "write");

	bzero(&msg, sizeof(msg));
	nbytes = read(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "read");

	code = msg.msg.clnge.code ^ 0xbadbeef20;

	/*
	 * Respond to challenge.
	 */
	bzero(&msg, sizeof(msg));
	msg.hdr.op = HVCTL_OP_RESPONSE;
	msg.hdr.seq = hvctl_seq++;
	msg.msg.clnge.code = code ^ 0x12cafe42a;
	nbytes = write(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "write");

	bzero(&msg, sizeof(msg));
	nbytes = read(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "read");
}

void
hv_close(void)
{
	close(hvctl_fd);
	hvctl_fd = -1;
}

void
hv_read(uint64_t addr, void *buf, size_t len)
{
	struct hv_io hi;

	hi.hi_cookie = addr;
	hi.hi_addr = buf;
	hi.hi_len = len;

	if (ioctl(hvctl_fd, HVIOCREAD, &hi) == -1)
		err(1, "ioctl");
}

void
hv_write(uint64_t addr, void *buf, size_t len)
{
	struct hv_io hi;

	hi.hi_cookie = addr;
	hi.hi_addr = buf;
	hi.hi_len = len;

	if (ioctl(hvctl_fd, HVIOCWRITE, &hi) == -1)
		err(1, "ioctl");
}
