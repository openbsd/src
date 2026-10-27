/*	$OpenBSD: build.c,v 1.1 2026/10/27 01:38:20 kevlo Exp $	*/

/*
 * Copyright (c) 2026 Kevin Lo <kevlo@openbsd.org>
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
#include <sys/param.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dev/pci/if_rgereg.h>

#include "rgemcode.h"

uint32_t	rge_maclen;
uint32_t	rge_phylen;

struct chunks {
	void		*start;
	uint32_t	*len;
};

struct chunks chunks_rtl8125[] = {
	{ rtl8125_mac_bps, &rge_maclen },
	{ mac_r25_mcu, &rge_phylen }
};

struct chunks chunks_rtl8125b[] = {
	{ rtl8125b_mac_bps, &rge_maclen },
	{ mac_r25b_mcu, &rge_phylen }
};

struct chunks chunks_rtl8125d_1[] = {
	{ NULL, &rge_maclen },
	{ mac_r25d_1_mcu, &rge_phylen }
};

struct chunks chunks_rtl8125d_2[] = {
	{ rtl8125d_2_mac_bps, &rge_maclen },
	{ mac_r25d_2_mcu, &rge_phylen }
};

struct chunks chunks_rtl8126_1[] = {
	{ rtl8126_1_mac_bps, &rge_maclen },
	{ mac_r26_1_mcu, &rge_phylen }
};

struct chunks chunks_rtl8126_2[] = {
	{ rtl8126_2_mac_bps, &rge_maclen },
	{ mac_r26_2_mcu, &rge_phylen }
};

struct chunks chunks_rtl8127[] = {
	{ rtl8127_mac_bps, &rge_maclen },
	{ mac_r27_mcu, &rge_phylen }
};

void
hswapn(uint16_t *p, int wcount)
{
	for (; wcount; wcount -=2) {
		*p = htole16(*p);
		p++;
	}
}

void
write_firmware(char *filename, void *header, size_t hlen, struct chunks *chunks,
    u_int nchunks)
{
	int fd, i, total;
	ssize_t rlen;

	printf("creating %s", filename);
	fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd == -1)
		err(1, "%s", filename);

	rlen = write(fd, header, hlen);
	if (rlen == -1)
		err(1, "%s", filename);
	if (rlen != hlen)
		errx(1, "%s: short write", filename);
	total = rlen;
	printf(" [%d", total);
	fflush(stdout);

	for (i = 0; i < nchunks; i++) {
		hswapn(chunks[i].start, *chunks[i].len);
		rlen = write(fd, chunks[i].start, *chunks[i].len);
		if (rlen == -1) {
			printf("\n");
			err(1, "%s", filename);
		}
		if (rlen != *chunks[i].len) {
			printf("\n");
			errx(1, "%s: short write", filename);
		}
		printf("+%zd", rlen);
		fflush(stdout);
		total += rlen;
	}

	printf("] total %d\n", total);

	close(fd);
}

int
main(int argc, char *argv[])
{
	struct rge_fw_hdr *fh;

	fh = (struct rge_fw_hdr *)malloc(sizeof *fh);

	memset(fh, 0, sizeof(*fh));
	rge_maclen = sizeof(rtl8125_mac_bps);
	rge_phylen = sizeof(mac_r25_mcu);
	fh->rge_maclen = htole32(rge_maclen);
	fh->rge_phylen = htole32(rge_phylen);
	write_firmware(RGE_RTL8125_FW, fh, sizeof(*fh), chunks_rtl8125,
	    nitems(chunks_rtl8125));

	memset(fh, 0, sizeof(*fh));
	rge_maclen = sizeof(rtl8125b_mac_bps);
	rge_phylen = sizeof(mac_r25b_mcu);
	fh->rge_maclen = htole32(rge_maclen);
	fh->rge_phylen = htole32(rge_phylen);
	write_firmware(RGE_RTL8125B_FW, fh, sizeof(*fh), chunks_rtl8125b,
	    nitems(chunks_rtl8125b));

	memset(fh, 0, sizeof(*fh));
	rge_maclen = 0;
	rge_phylen = sizeof(mac_r25d_1_mcu);
	fh->rge_maclen = htole32(rge_maclen);
	fh->rge_phylen = htole32(rge_phylen);
	write_firmware(RGE_RTL8125D_1_FW, fh, sizeof(*fh), chunks_rtl8125d_1,
	    nitems(chunks_rtl8125d_1));

	memset(fh, 0, sizeof(*fh));
	rge_maclen = sizeof(rtl8125d_2_mac_bps);
	rge_phylen = sizeof(mac_r25d_2_mcu);
	fh->rge_maclen = htole32(rge_maclen);
	fh->rge_phylen = htole32(rge_phylen);
	write_firmware(RGE_RTL8125D_2_FW, fh, sizeof(*fh), chunks_rtl8125d_2,
	    nitems(chunks_rtl8125d_2));

	memset(fh, 0, sizeof(*fh));
	rge_maclen = sizeof(rtl8126_1_mac_bps);
	rge_phylen = sizeof(mac_r26_1_mcu);
	fh->rge_maclen = htole32(rge_maclen);
	fh->rge_phylen = htole32(rge_phylen);
	write_firmware(RGE_RTL8126_1_FW, fh, sizeof(*fh), chunks_rtl8126_1,
	    nitems(chunks_rtl8126_1));

	memset(fh, 0, sizeof(*fh));
	rge_maclen = sizeof(rtl8126_2_mac_bps);
	rge_phylen = sizeof(mac_r26_2_mcu);
	fh->rge_maclen = htole32(rge_maclen);
	fh->rge_phylen = htole32(rge_phylen);
	write_firmware(RGE_RTL8126_2_FW, fh, sizeof(*fh), chunks_rtl8126_2,
	    nitems(chunks_rtl8126_2));

	memset(fh, 0, sizeof(*fh));
	rge_maclen = sizeof(rtl8127_mac_bps);
	rge_phylen = sizeof(mac_r27_mcu);
	fh->rge_maclen = htole32(rge_maclen);
	fh->rge_phylen = htole32(rge_phylen);
	write_firmware(RGE_RTL8127_FW, fh, sizeof(*fh), chunks_rtl8127,
	    nitems(chunks_rtl8127));

	free(fh);

	return 0;
}
