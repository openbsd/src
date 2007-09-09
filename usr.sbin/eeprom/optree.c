/*	$OpenBSD: optree.c,v 1.2 2007/09/09 14:19:28 fgsch Exp $	*/

/*
 * Copyright (c) 2007 Federico G. Schwindt <fgsch@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/openpromio.h>
#include <sys/ioctl.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern  char *path_openprom;

static void
op_print(struct opiocdesc *opio, int depth)
{
	char *p;
	int i, special;

	opio->op_name[opio->op_namelen] = '\0';
	printf("%*s%s: ", depth * 4, " ", opio->op_name);
	if (opio->op_buflen > 0) {
		opio->op_buf[opio->op_buflen] = '\0';
		special = 0;

		if (opio->op_buf[0] != '\0') {
			for (i = 0; i < opio->op_buflen; i++) {
				p = &opio->op_buf[i];
				if ((*p < ' ' || *p > '~') && (*p != '\0' ||
				    (i + 1 < opio->op_buflen &&
				    (*++p < ' ' || *p > '~')))) {
					special = 1;
					break;
				}
			}
		} else {
			if (opio->op_buflen > 1)
				special = 1;
		}

		if (special) {
			for (i = 0; opio->op_buflen - i >= sizeof(int);
			    i += sizeof(int)) {
				if (i)
					printf(".");
				printf("%08x", *(int *)(long)&opio->op_buf[i]);
			}
			if (i < opio->op_buflen) {
				if (i)
					printf(".");
				for (; i < opio->op_buflen; i++) {
					printf("%02x",
					    *(u_char *)&opio->op_buf[i]);
				}
			}
		} else {
			for (i = 0; i < opio->op_buflen;
			    i += strlen(&opio->op_buf[i]) + 1) {
				if (i)
					printf(" + ");
				printf("'%s'", &opio->op_buf[i]);
			}
		}
	}
	printf("\n");
}

void
op_nodes(int fd, int node, int depth)
{
	char op_buf[BUFSIZ * 4];
	char op_name[BUFSIZ];
	struct opiocdesc opio;

	opio.op_nodeid = node;
	opio.op_buf = op_buf;
	opio.op_name = op_name;

	if (!node) {
		if (ioctl(fd, OPIOCGETNEXT, &opio) < 0)
			err(1, "OPIOCGETNEXT");
		node = opio.op_nodeid;
	} else
		printf("\n%*s", depth * 4, " ");

	printf("Node 0x%x\n", node);

	for (;;) {
		opio.op_buflen = sizeof(op_buf);
		opio.op_namelen = sizeof(op_name);

		/* Get the next property. */
		if (ioctl(fd, OPIOCNEXTPROP, &opio) < 0)
			err(1, "OPIOCNEXTPROP");

		op_buf[opio.op_buflen] = '\0';
		(void)strlcpy(op_name, op_buf, sizeof(op_name));
		opio.op_namelen = strlen(op_name);

		/* If it's the last, punt. */
		if (opio.op_namelen == 0)
			break;

		bzero(op_buf, sizeof(op_buf));
		opio.op_buflen = sizeof(op_buf);

		/* And its value. */
		if (ioctl(fd, OPIOCGET, &opio) < 0)
			err(1, "OPIOCGET");

		op_print(&opio, depth + 1);
	}

	/* Get next child. */
	if (ioctl(fd, OPIOCGETCHILD, &opio) < 0)
		err(1, "OPIOCGETCHILD");
	if (opio.op_nodeid)
		op_nodes(fd, opio.op_nodeid, depth + 1);

	/* Get next node/sibling. */
	opio.op_nodeid = node;
	if (ioctl(fd, OPIOCGETNEXT, &opio) < 0)
		err(1, "OPIOCGETNEXT");
	if (opio.op_nodeid)
		op_nodes(fd, opio.op_nodeid, depth);
}

void
op_tree(void)
{
	int fd;

	if ((fd = open(path_openprom, O_RDONLY, 0640)) < 0)
		err(1, "open: %s", path_openprom);
	op_nodes(fd, 0, 0);
	(void)close(fd);
}
