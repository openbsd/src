#include <sys/limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/scsiio.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "extern.h"

#define WAVHDRLEN 44
extern int errno;
extern int fd;
extern char *cdname;
extern char *track_types;

int
blank(void)
{
	scsireq_t scr;
	int r;

	bzero(&scr, sizeof(scr));
	scr.cmd[0] = 0xa1;
	scr.cmd[1] = 0x01;
	scr.cmdlen = 12;
	scr.datalen = 0;
	scr.timeout = 120000;
	scr.flags = SCCMD_ESCAPE;
	scr.senselen = SENSEBUFLEN;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	if (r == -1 && errno == EPERM) {
		close(fd);
		fd = -1;
		if (! open_cd(cdname, 1))
			return (-1);
	}
	r = ioctl(fd, SCIOCCOMMAND, &scr);
	return (r == 0 ? scr.retsts : -1);
}

int
unit_ready(void)
{
	scsireq_t scr;
	int r;

	bzero(&scr, sizeof(scr));
	scr.cmd[0] = 0x00;
	scr.cmdlen = 6;
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
	scsireq_t scr;
	int r;

	bzero(&scr, sizeof(scr));
	scr.cmd[0] = 0x35;
	scr.cmdlen = 10;
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
	scsireq_t scr;
	int r;

	bzero(&scr, sizeof(scr));
	scr.cmd[0] = 0x5b;
	scr.cmd[2] = 0x02; /* close session */
	scr.cmd[5] = 0x00; /* track number */
	scr.cmdlen = 10;
	scr.datalen = 0;
	scr.timeout = 120000;
	scr.flags = SCCMD_ESCAPE;
	scr.senselen = SENSEBUFLEN;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	return (r == 0 ? scr.retsts : -1);

}

int
writetao(int ntracks, char *track_files[])
{
	u_char modebuf[70];
	u_int blklen;
	u_int t;
	int i,r;
	u_char bdlen;
	
	if (track_types == NULL)
		track_types = strdup("d");
	if ((r = mode_sense_write(modebuf)) != SCCMD_OK) {
		warnx("mode sense failed: %d", r);
		return (r);
	}
	bdlen = modebuf[7];
	modebuf[2+8+bdlen] |= 0x40; /* Buffer Underrun Free Enable */
	modebuf[2+8+bdlen] |= 0x01; /* change write type to TAO */

	for (i = 0, t = 0; t < ntracks; t++) {
		if (track_types[i] == 'd') {
			modebuf[3+8+bdlen] = 0x04; /* track mode = data */
			modebuf[4+8+bdlen] = 0x08; /* 2048 block track mode */
			modebuf[8+8+bdlen] = 0x00; /* turn off XA */
			blklen = 2048;
		} else if (track_types[i] == 'a') {
			modebuf[3+8+bdlen] = 0x00; /* track mode = audio */
			modebuf[4+8+bdlen] = 0x00; /* 2352 block track mode */
			modebuf[8+8+bdlen] = 0x00; /* turn off XA */
			blklen = 2352;
		} else {
			warnx("invalid track type specified");
			return (1);
		}
		while (unit_ready() != SCCMD_OK)
			continue;
		if ((r = mode_select_write(modebuf)) != SCCMD_OK) {
			warnx("mode select failed: %d",r);
			return (r);
		}
		writetrack(track_files[t], blklen, t, track_types[i]);
		synchronize_cache();
		if (track_types[i+1] != '\0')
			i++;
	}
	fprintf(stderr,"\n");
	synchronize_cache();
	close_session();
	return (0);
}

int
writetrack(char *file, u_int blklen, u_int trackno, char type)
{
	u_char databuf[65536];
	struct stat sb;
	scsireq_t scr;
	u_int end_lba, lba;
	u_int tmp;
	int r,rfd;
	u_char nblk;

	nblk = 65535/blklen;
	bzero(&scr, sizeof(scr));
	scr.timeout = 300000;
	scr.cmd[0] = 0x2a;
	scr.cmd[1] = 0x00;
	scr.cmd[8] = nblk; /* Transfer length in blocks (LSB) */
	scr.cmdlen = 10;
	scr.databuf = (caddr_t)databuf;
	scr.datalen = nblk * blklen;
	scr.senselen = SENSEBUFLEN;
	scr.flags = SCCMD_ESCAPE|SCCMD_WRITE;

	if (get_nwa(&lba) != SCCMD_OK) {
		warnx("cannot get next writable address");
		return (-1);
	}
	tmp = htobe32(lba); /* update lba in cdb */
	memcpy(&scr.cmd[2], &tmp, sizeof(tmp));

	if (stat(file, &sb) != 0) {
		warn("cannot stat file %s",file);
		return (-1);
	}
	if (sb.st_size / blklen + 1 > UINT_MAX || sb.st_size < blklen) {
		warnx("file %s has invalid size",file);
		return (-1);
	}
	if (type == 'a')
		sb.st_size -= WAVHDRLEN;
	if (sb.st_size % blklen) {
		warnx("file %s is not multiple of block length %d",file,blklen);
		end_lba = sb.st_size / blklen + lba + 1;
	} else {
		end_lba = sb.st_size / blklen + lba;
	}
	rfd = open(file, O_RDONLY, 0640);
	if (type == 'a') {
		if (lseek(rfd, WAVHDRLEN, SEEK_SET) == -1)
			err(1, "seek failed");
	}
	while ((lba < end_lba) && (nblk != 0)) {
		while (lba + nblk <= end_lba) {
			read(rfd, databuf, nblk * blklen);
			scr.cmd[8] = nblk;
			scr.datalen = nblk * blklen;
			r = ioctl(fd, SCIOCCOMMAND, &scr);
			if (r != 0) {
				warn("ioctl failed while attempting to write");
				return (-1);
			}
			if (scr.retsts != SCCMD_OK) {
				warnx("ioctl returned bad status while attempting to write: %d", scr.retsts);
				return (r);
			}
			lba += nblk;
			fprintf(stderr,"\rLBA: 0x%06x/0x%06x",lba,end_lba);
			tmp = htobe32(lba); /* update lba in cdb */
			memcpy(&scr.cmd[2], &tmp, sizeof(tmp));
		}
		nblk--;
	}
	close(rfd);
	return (0);
}

int
mode_sense_write(unsigned char buf[])
{
	scsireq_t scr;
	int r;

	bzero(&scr, sizeof(scr));
	scr.timeout = 4000;
	scr.senselen = SENSEBUFLEN;
	scr.cmd[0] = 0x5a;
	scr.cmd[1] = 0x00;
	scr.cmd[2] = 0x05; /* Write parameters mode page */
	scr.cmd[7] = 0x00;
	scr.cmd[8] = 0x46; /* 16 for the header + size from pg. 89 mmc-r10a.pdf */
	scr.cmdlen = 10;
	scr.datalen= 0x46; 
	scr.flags = SCCMD_ESCAPE|SCCMD_READ;
	scr.databuf = (caddr_t)buf;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	return (r == 0 ? scr.retsts : -1);
}

int
mode_select_write(unsigned char buf[])
{
	scsireq_t scr;
	int r;

	bzero(&scr, sizeof(scr));
	scr.timeout = 4000;
	scr.senselen = SENSEBUFLEN;
	scr.cmd[0] = 0x55;
	scr.cmd[1] = 0x10; /* pages aren't vendor specific */
	scr.cmd[2] = 0x00;
	scr.cmd[7] = 0x00;
	scr.cmd[8] = 2 + buf[1] + 256 * buf[0];
	scr.cmdlen = 10;
	scr.datalen = 2 + buf[1] + 256 * buf[0];
	scr.flags = SCCMD_ESCAPE|SCCMD_WRITE;
	scr.databuf = (caddr_t)buf;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	return (r == 0 ? scr.retsts : -1);
}

int
get_nwa(int *nwa)
{
	u_char databuf[28];
	scsireq_t scr;
	int r,tmp;

	bzero(&scr, sizeof(scr));
	scr.timeout = 4000;
	scr.senselen = SENSEBUFLEN;
	scr.cmd[0] = 0x52; /* READ TRACK INFO */
	scr.cmd[1] = 0x01;
	scr.cmd[5] = 0xff; /* Invisible Track */
	scr.cmd[2] = 0x05; /* Write parameters mode page */
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
