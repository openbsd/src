/*	$OpenBSD: mmc.c,v 1.28 2010/03/01 02:09:44 krw Exp $	*/
/*
 * Copyright (c) 2006 Michael Coulter <mjc@openbsd.org>
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

#include <sys/limits.h>
#include <sys/types.h>
#include <sys/scsiio.h>
#include <sys/param.h>
#include <scsi/cd.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "extern.h"

extern int fd;
extern u_int8_t mediacap[];
extern char *cdname;
extern int verbose;

#define SCSI_GET_CONFIGURATION		0x46

#define MMC_FEATURE_HDR_LEN		8

static const struct {
	u_int16_t id;
	char *name;
} mmc_feature[] = {
	{ 0x0000, "Profile List" },
	{ 0x0001, "Core" },
	{ 0x0002, "Morphing" },
	{ 0x0003, "Removable Medium" },
	{ 0x0004, "Write Protect" },
	{ 0x0010, "Random Readable" },
	{ 0x001d, "Multi-Read" },
	{ 0x001e, "CD Read" },
	{ 0x001f, "DVD Read" },
	{ 0x0020, "Random Writable" },
	{ 0x0021, "Incremental Streaming Writable" },
	{ 0x0022, "Sector Erasable" },
	{ 0x0023, "Formattable" },
	{ 0x0024, "Hardware Defect Management" },
	{ 0x0025, "Write Once" },
	{ 0x0026, "Restricted Overwrite" },
	{ 0x0027, "CD-RW CAV Write" },
	{ 0x0028, "MRW" },
	{ 0x0029, "Enhanced Defect Reporting" },
	{ 0x002a, "DVD+RW" },
	{ 0x002b, "DVD+R" },
	{ 0x002c, "Rigid Restricted Overwrite" },
	{ 0x002d, "CD Track at Once (TAO)" },
	{ 0x002e, "CD Mastering (Session at Once)" },
	{ 0x002f, "DVD-RW Write" },
	{ 0x0030, "DDCD-ROM (Legacy)" },
	{ 0x0031, "DDCD-R (Legacy)" },
	{ 0x0032, "DDCD-RW (Legacy)" },
	{ 0x0033, "Layer Jump Recording" },
	{ 0x0037, "CD-RW Media Write Support" },
	{ 0x0038, "BD-R Pseudo-Overwrite (POW)" },
	{ 0x003a, "DVD+RW Dual Layer" },
	{ 0x003b, "DVD+R Dual Layer" },
	{ 0x0040, "BD Read" },
	{ 0x0041, "BD Write" },
	{ 0x0042, "Timely Safe Recording (TSR)" },
	{ 0x0050, "HD DVD Read" },
	{ 0x0051, "HD DVD Write" },
	{ 0x0080, "Hybrid Disc" },
	{ 0x0100, "Power Management" },
	{ 0x0101, "S.M.A.R.T." },
	{ 0x0102, "Embedded Changer" },
	{ 0x0103, "CD Audio External Play (Legacy)" },
	{ 0x0104, "Microcode Upgrade" },
	{ 0x0105, "Timeout" },
	{ 0x0106, "DVD CSS" },
	{ 0x0107, "Real Time Streaming" },
	{ 0x0108, "Drive Serial Number" },
	{ 0x0109, "Media Serial Number" },
	{ 0x010a, "Disc Control Blocks (DCBs)" },
	{ 0x010b, "DVD CPRM" },
	{ 0x010c, "Firmware Information" },
	{ 0x010d, "AACS" },
	{ 0x0110, "VCPS" },
	{ 0, NULL }
};

static const struct {
	u_int16_t id;
	char *name;
} mmc_profile[] = {
	{ 0x0001, "Re-writable disk, capable of changing behaviour" },
	{ 0x0002, "Re-writable, with removable media" },
	{ 0x0003, "Magneto-Optical disk with sector erase capability" },
	{ 0x0004, "Optical write once" },
	{ 0x0005, "Advance Storage -- Magneto-Optical" },
	{ 0x0008, "Read only Compact Disc" },
	{ 0x0009, "Write once Compact Disc" },
	{ 0x000a, "Re-writable Compact Disc" },
	{ 0x0010, "Read only DVD" },
	{ 0x0011, "Write once DVD using Sequential recording" },
	{ 0x0012, "Re-writable DVD" },
	{ 0x0013, "Re-recordable DVD using Restricted Overwrite" },
	{ 0x0014, "Re-recordable DVD using Sequential recording" },
	{ 0x0015, "Dual Layer DVD-R using Sequential recording" },
	{ 0x0016, "Dual Layer DVD-R using Layer Jump recording" },
	{ 0x001a, "DVD+ReWritable" },
	{ 0x001b, "DVD+Recordable" },
	{ 0x0020, "DDCD-ROM" },
	{ 0x0021, "DDCD-R" },
	{ 0x0022, "DDCD-RW" },
	{ 0x002a, "DVD+Rewritable Dual Layer" },
	{ 0x002b, "DVD+Recordable Dual Layer" },
	{ 0x003e, "Blu-ray Disc ROM" },
	{ 0x003f, "Blu-ray Disc Recordable -- Sequential Recording Mode" },
	{ 0x0040, "Blu-ray Disc Recordable -- Random Recording Mode" },
	{ 0x0041, "Blu-ray Disc Rewritable" },
	{ 0x004e, "Read-only HD DVD" },
	{ 0x004f, "Write-once HD DVD" },
	{ 0x0050, "Rewritable HD DVD" },
	{ 0, NULL }
};

int
get_media_type(void)
{
	scsireq_t scr;
	char buf[32];
	u_char disctype;
	int rv, error;

	rv = MEDIATYPE_UNKNOWN;
	memset(buf, 0, sizeof(buf));
	memset(&scr, 0, sizeof(scr));

	scr.cmd[0] = READ_TOC;
	scr.cmd[1] = 0x2;	/* MSF */
	scr.cmd[2] = 0x4;	/* ATIP */
	scr.cmd[8] = 0x20;

	scr.flags = SCCMD_ESCAPE | SCCMD_READ;
	scr.databuf = buf;
	scr.datalen = sizeof(buf);
	scr.cmdlen = 10;
	scr.timeout = 120000;
	scr.senselen = SENSEBUFLEN;

	error = ioctl(fd, SCIOCCOMMAND, &scr);
	if (error != -1 && scr.retsts == SCCMD_OK && scr.datalen_used > 7) {
		disctype = (buf[6] >> 6) & 0x1;
		if (disctype == 0)
			rv = MEDIATYPE_CDR;
		else if (disctype == 1)
			rv = MEDIATYPE_CDRW;
	}

	return (rv);
}

int
get_media_capabilities(u_int8_t *cap, int rt)
{
	scsireq_t scr;
	u_char buf[4096];
	u_int32_t i, dlen;
	u_int16_t feature, profile, tmp;
	u_int8_t feature_len;
	int current, error, j, k;

	memset(cap, 0, MMC_FEATURE_MAX / NBBY);
	memset(buf, 0, sizeof(buf));
	memset(&scr, 0, sizeof(scr));

	scr.cmd[0] = SCSI_GET_CONFIGURATION;
	scr.cmd[1] = rt;
	tmp = htobe16(sizeof(buf));
	memcpy(scr.cmd + 7, &tmp, sizeof(u_int16_t));

	scr.flags = SCCMD_ESCAPE | SCCMD_READ;
	scr.databuf = buf;
	scr.datalen = sizeof(buf);
	scr.cmdlen = 10;
	scr.timeout = 120000;
	scr.senselen = SENSEBUFLEN;

	error = ioctl(fd, SCIOCCOMMAND, &scr);
	if (error == -1 || scr.retsts != SCCMD_OK)
		return (-1);
	if (scr.datalen_used < MMC_FEATURE_HDR_LEN)
		return (-1);	/* Can't get the header. */

	/* Include the whole header in the length. */
	dlen = betoh32(*(u_int32_t *)buf) + 4;
	if (dlen > scr.datalen_used)
		dlen = scr.datalen_used;

	if (verbose > 1)
		printf("Features:\n");
	for (i = MMC_FEATURE_HDR_LEN; i + 3 < dlen; i += feature_len) {
		feature_len = buf[i + 3] + 4;
		if (feature_len + i > dlen)
			break;

		feature = betoh16(*(u_int16_t *)(buf + i));
		if (feature >= MMC_FEATURE_MAX)
			break;

		if (verbose > 1) {
			printf("0x%04x", feature);
			for (j = 0; mmc_feature[j].name != NULL; j++)
				if (feature == mmc_feature[j].id)
					break;
			if (mmc_feature[j].name == NULL)
				printf(" <Undocumented>");
			else
				printf(" %s", mmc_feature[j].name);
			if (feature_len > 4)
				printf(" (%d bytes of data)", feature_len - 4);
			printf("\n");
			if (verbose > 2) {
				printf("    ");
				for (j = i; j < i + feature_len; j++) {
					printf("%02x", buf[j]);
					if ((j + 1) == (i + feature_len))
						printf("\n");
					else if ((j > i) && ((j - i + 1) % 16
					    == 0))
						printf("\n    ");
					else if ((j - i) == 3)
						printf("|");
					else
						printf(" ");
				}
			}
		}
		if (feature == 0 && verbose > 1) {
			if (verbose > 2)
				printf("    Profiles:\n");
			for (j = i + 4; j < i + feature_len; j += 4) {
				profile = betoh16(*(u_int16_t *)(buf+j));
				current = buf[j+2] == 1;
				if (verbose < 3 && !current)
					continue;
				if (current)
					printf("  * ");
				else
					printf("    ");
				printf("0x%04x", profile);
				for (k = 0; mmc_profile[k].name != NULL; k++)
					if (profile == mmc_profile[k].id)
						break;
				if (mmc_profile[k].name == NULL)
					printf(" <Undocumented>");
				else
					printf(" %s", mmc_profile[k].name);
				printf(" %s\n", current ? "[Current Profile]" :
				    "" );
			}
		}
		setbit(cap, feature);
	}

	return (0);
}

int
set_speed(int wspeed)
{
	scsireq_t scr;
	int r;

	memset(&scr, 0, sizeof(scr));
	scr.cmd[0] = SET_CD_SPEED;
	scr.cmd[1] = (isset(mediacap, MMC_FEATURE_CDRW_CAV)) != 0;
	*(u_int16_t *)(scr.cmd + 2) = htobe16(DRIVE_SPEED_OPTIMAL);
	*(u_int16_t *)(scr.cmd + 4) = htobe16(wspeed);

	scr.cmdlen = 12;
	scr.datalen = 0;
	scr.timeout = 120000;
	scr.flags = SCCMD_ESCAPE;
	scr.senselen = SENSEBUFLEN;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	return (r == 0 ? scr.retsts : -1);
}

int
blank(void)
{
	struct scsi_blank *scb;
	scsireq_t scr;
	int r;

	bzero(&scr, sizeof(scr));
	scb = (struct scsi_blank *)scr.cmd;
	scb->opcode = BLANK;
	scb->byte2 |= BLANK_MINIMAL;
	scr.cmdlen = sizeof(*scb);
	scr.datalen = 0;
	scr.timeout = 120000;
	scr.flags = SCCMD_ESCAPE;
	scr.senselen = SENSEBUFLEN;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	return (r == 0 ? scr.retsts : -1);
}

int
unit_ready(void)
{
	struct scsi_test_unit_ready *scb;
	scsireq_t scr;
	int r;

	bzero(&scr, sizeof(scr));
	scb = (struct scsi_test_unit_ready *)scr.cmd;
	scb->opcode = TEST_UNIT_READY;
	scr.cmdlen = sizeof(*scb);
	scr.datalen = 0;
	scr.timeout = 120000;
	scr.flags = SCCMD_ESCAPE;
	scr.senselen = SENSEBUFLEN;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	return (r == 0 ? scr.retsts : -1);
}

int
synchronize_cache(void)
{
	struct scsi_synchronize_cache *scb;
	scsireq_t scr;
	int r;

	bzero(&scr, sizeof(scr));
	scb = (struct scsi_synchronize_cache *)scr.cmd;
	scb->opcode = SYNCHRONIZE_CACHE;
	scr.cmdlen = sizeof(*scb);
	scr.datalen = 0;
	scr.timeout = 120000;
	scr.flags = SCCMD_ESCAPE;
	scr.senselen = SENSEBUFLEN;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	return (r == 0 ? scr.retsts : -1);
}

int
close_session(void)
{
	struct scsi_close_track *scb;
	scsireq_t scr;
	int r;

	bzero(&scr, sizeof(scr));
	scb = (struct scsi_close_track *)scr.cmd;
	scb->opcode = CLOSE_TRACK;
	scb->closefunc = CT_CLOSE_SESS;
	scr.cmdlen = sizeof(*scb);
	scr.datalen = 0;
	scr.timeout = 120000;
	scr.flags = SCCMD_ESCAPE;
	scr.senselen = SENSEBUFLEN;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	return (r == 0 ? scr.retsts : -1);
}

int
writetao(struct track_head *thp)
{
	u_char modebuf[70], bdlen;
	struct track_info *tr;
	int r, track = 0;

	if ((r = mode_sense_write(modebuf)) != SCCMD_OK) {
		warnx("mode sense failed: %d", r);
		return (r);
	}
	bdlen = modebuf[7];
	modebuf[2+8+bdlen] |= 0x40; /* Buffer Underrun Free Enable */
	modebuf[2+8+bdlen] |= 0x01; /* change write type to TAO */

	SLIST_FOREACH(tr, thp, track_list) {
		track++;
		switch (tr->type) {
		case 'd':
			modebuf[3+8+bdlen] = 0x04; /* track mode = data */
			modebuf[4+8+bdlen] = 0x08; /* 2048 block track mode */
			modebuf[8+8+bdlen] = 0x00; /* turn off XA */
			break;
		case 'a':
			modebuf[3+8+bdlen] = 0x00; /* track mode = audio */
			modebuf[4+8+bdlen] = 0x00; /* 2352 block track mode */
			modebuf[8+8+bdlen] = 0x00; /* turn off XA */
			break;
		default:
			warn("impossible tracktype detected");
			break;
		}
		while (unit_ready() != SCCMD_OK)
			continue;
		if ((r = mode_select_write(modebuf)) != SCCMD_OK) {
			warnx("mode select failed: %d", r);
			return (r);
		}

		set_speed(tr->speed);
		writetrack(tr, track);
		synchronize_cache();
	}
	fprintf(stderr, "Closing session.\n");
	close_session();
	return (0);
}

int
writetrack(struct track_info *tr, int track)
{
	struct timeval tv, otv, atv;
	u_char databuf[65536], nblk;
	u_int end_lba, lba, tmp;
	scsireq_t scr;
	int r;

	nblk = 65535/tr->blklen;
	bzero(&scr, sizeof(scr));
	scr.timeout = 300000;
	scr.cmd[0] = WRITE_BIG;
	scr.cmd[1] = 0x00;
	scr.cmd[8] = nblk; /* Transfer length in blocks (LSB) */
	scr.cmdlen = 10;
	scr.databuf = (caddr_t)databuf;
	scr.datalen = nblk * tr->blklen;
	scr.senselen = SENSEBUFLEN;
	scr.flags = SCCMD_ESCAPE|SCCMD_WRITE;

	timerclear(&otv);
	atv.tv_sec = 1;
	atv.tv_usec = 0;

	if (get_nwa(&lba) != SCCMD_OK) {
		warnx("cannot get next writable address");
		return (-1);
	}
	tmp = htobe32(lba); /* update lba in cdb */
	memcpy(&scr.cmd[2], &tmp, sizeof(tmp));

	if (tr->sz / tr->blklen + 1 > UINT_MAX || tr->sz < tr->blklen) {
		warnx("file %s has invalid size", tr->file);
		return (-1);
	}
	if (tr->sz % tr->blklen) {
		warnx("file %s is not multiple of block length %d",
		    tr->file, tr->blklen);
		end_lba = tr->sz / tr->blklen + lba + 1;
	} else {
		end_lba = tr->sz / tr->blklen + lba;
	}
	if (lseek(tr->fd, tr->off, SEEK_SET) == -1)
		err(1, "seek failed for file %s", tr->file);
	while (lba < end_lba && nblk != 0) {
		while (lba + nblk <= end_lba) {
			read(tr->fd, databuf, nblk * tr->blklen);
			scr.cmd[8] = nblk;
			scr.datalen = nblk * tr->blklen;
again:
			r = ioctl(fd, SCIOCCOMMAND, &scr);
			if (r != 0) {
				printf("\r%60s", "");
				warn("ioctl failed while attempting to write");
				return (-1);
			}
			if (scr.retsts == SCCMD_SENSE && scr.sense[2] == 0x2) {
				usleep(1000);
				goto again;
			}
			if (scr.retsts != SCCMD_OK) {
				printf("\r%60s", "");
				warnx("ioctl returned bad status while "
				    "attempting to write: %d",
				    scr.retsts);
				return (r);
			}
			lba += nblk;

			gettimeofday(&tv, NULL);
			if (lba == end_lba || timercmp(&tv, &otv, >)) {
				fprintf(stderr,
				    "\rtrack %02d '%c' %08u/%08u %3d%%",
				    track, tr->type,
				    lba, end_lba, 100 * lba / end_lba);
				timeradd(&tv, &atv, &otv);
			}
			tmp = htobe32(lba); /* update lba in cdb */
			memcpy(&scr.cmd[2], &tmp, sizeof(tmp));
		}
		nblk--;
	}
	printf("\n");
	close(tr->fd);
	return (0);
}

int
mode_sense_write(unsigned char buf[])
{
	struct scsi_mode_sense_big *scb;
	scsireq_t scr;
	int r;

	bzero(&scr, sizeof(scr));
	scb = (struct scsi_mode_sense_big *)scr.cmd;
	scb->opcode = MODE_SENSE_BIG;
	/* XXX: need to set disable block descriptors and check SCSI drive */
	scb->page = WRITE_PARAM_PAGE;
	scb->length[1] = 0x46; /* 16 for the header + size from pg. 89 mmc-r10a.pdf */
	scr.cmdlen = sizeof(*scb);
	scr.timeout = 4000;
	scr.senselen = SENSEBUFLEN;
	scr.datalen= 0x46;
	scr.flags = SCCMD_ESCAPE|SCCMD_READ;
	scr.databuf = (caddr_t)buf;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	return (r == 0 ? scr.retsts : -1);
}

int
mode_select_write(unsigned char buf[])
{
	struct scsi_mode_select_big *scb;
	scsireq_t scr;
	int r;

	bzero(&scr, sizeof(scr));
	scb = (struct scsi_mode_select_big *)scr.cmd;
	scb->opcode = MODE_SELECT_BIG;

	/*
	 * INF-8020 says bit 4 in byte 2 is '1'
	 * INF-8090 refers to it as 'PF(1)' then doesn't
	 * describe it.
	 */
	scb->byte2 = 0x10;
	scb->length[1] = 2 + buf[1] + 256 * buf[0];
	scr.timeout = 4000;
	scr.senselen = SENSEBUFLEN;
	scr.cmdlen = sizeof(*scb);
	scr.datalen = 2 + buf[1] + 256 * buf[0];
	scr.flags = SCCMD_ESCAPE|SCCMD_WRITE;
	scr.databuf = (caddr_t)buf;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	return (r == 0 ? scr.retsts : -1);
}

int
get_disc_size(off_t *availblk)
{
	u_char databuf[28];
	struct scsi_read_track_info *scb;
	scsireq_t scr;
	int r, tmp;

	bzero(&scr, sizeof(scr));
	scb = (struct scsi_read_track_info *)scr.cmd;
	scr.timeout = 4000;
	scr.senselen = SENSEBUFLEN;
	scb->opcode = READ_TRACK_INFO;
	scb->addrtype = RTI_TRACK;
	scb->addr[3] = 1;
	scb->data_len[1] = 0x1c;
	scr.cmdlen = sizeof(*scb);
	scr.datalen= 0x1c;
	scr.flags = SCCMD_ESCAPE|SCCMD_READ;
	scr.databuf = (caddr_t)databuf;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	memcpy(&tmp, &databuf[16], sizeof(tmp));
	*availblk = betoh32(tmp);
	return (r == 0 ? scr.retsts : -1);
}

int
get_nwa(int *nwa)
{
	u_char databuf[28];
	scsireq_t scr;
	int r, tmp;

	bzero(&scr, sizeof(scr));
	scr.timeout = 4000;
	scr.senselen = SENSEBUFLEN;
	scr.cmd[0] = READ_TRACK_INFO;
	scr.cmd[1] = 0x01;
	scr.cmd[5] = 0xff; /* Invisible Track */
	scr.cmd[7] = 0x00;
	scr.cmd[8] = 0x1c;
	scr.cmdlen = 10;
	scr.datalen= 0x1c;
	scr.flags = SCCMD_ESCAPE|SCCMD_READ;
	scr.databuf = (caddr_t)databuf;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	memcpy(&tmp, &databuf[12], sizeof(tmp));
	*nwa = betoh32(tmp);
	return (r == 0 ? scr.retsts : -1);
}
