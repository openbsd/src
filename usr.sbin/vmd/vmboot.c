/*	$OpenBSD: vmboot.c,v 1.7 2018/11/26 10:39:30 reyk Exp $	*/

/*
 * Copyright (c) 2016 Reyk Floeter <reyk@openbsd.org>
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

#include <sys/param.h>	/* DEV_BSIZE roundup */
#include <sys/reboot.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/disklabel.h>

#include <ufs/ffs/fs.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <err.h>
#include <vis.h>

#include "vmd.h"
#include "vmboot.h"
#include "virtio.h"

int	 vmboot_bootconf(char *, size_t, struct vmboot_params *);
int	 vmboot_bootcmd(char *, struct vmboot_params *);
int	 vmboot_bootargs(int argc, char **argv, struct vmboot_params *);
uint32_t vmboot_bootdevice(const char *);

int	 vmboot_strategy(void *, int, daddr32_t, size_t, void *, size_t *);
off_t	 vmboot_findopenbsd(struct open_file *, off_t, struct disklabel *);
void	*vmboot_loadfile(struct open_file *, char *, size_t *);

int
vmboot_bootcmd(char *line, struct vmboot_params *bp)
{
	char *p, *args[16];
	int ac = 0;
	char *last;

	for (args[0] = NULL, (p = strtok_r(line, " ", &last)); p;
	    (p = strtok_r(NULL, " ", &last))) {
		if (ac < (int)(sizeof(args) / sizeof(args[0])) - 1)
			args[ac++] = p;
	}
	if (ac == 0)
		return (0);
	args[ac] = NULL;

	/*
	 * Subset of boot.conf(8) options
	 */
	if (strcmp("boot", args[0]) == 0)
		return (vmboot_bootargs(ac, args, bp));
	else if (strcmp("set", args[0]) == 0) {
		if (ac < 3)
			return (-1);
		if (strcmp("device", args[1]) == 0) {
			if ((size_t)strnvis(bp->vbp_device, args[2],
			    sizeof(bp->vbp_device), VIS_SAFE) >=
			    sizeof(bp->vbp_device)) {
				log_warnx("invalid device name");
				return (-1);
			}
		} else if (strcmp("image", args[1]) == 0) {
			if ((size_t)strnvis(bp->vbp_image, args[2],
			    sizeof(bp->vbp_image), VIS_SAFE) >=
			    sizeof(bp->vbp_image)) {
				log_warnx("invalid image name");
				return (-1);
			}
		}
	}

	return (0);
}

int
vmboot_bootargs(int ac, char **av, struct vmboot_params *bp)
{
	char *p;
	int ch;

	if (ac < 2)
		return (0);

	/*
	 * Syntax is based on boot(8): boot "[hd0a[:/file]] [-asdc]"
	 */
	if (*av[1] != '-') {
		if ((p = strchr(av[1], ':')) != NULL) {
			*p++ = '\0';
			if (!strlen(p)) {
				log_warnx("invalid file syntax");
				return (-1);
			}
			if ((size_t)strnvis(bp->vbp_device, av[1],
			    sizeof(bp->vbp_device), VIS_SAFE) >=
			    sizeof(bp->vbp_device)) {
				log_warnx("invalid device name");
				return (-1);
			}
		} else {
			p = av[1];
		}
		if ((size_t)strnvis(bp->vbp_image, p,
		    sizeof(bp->vbp_image), VIS_SAFE) >= sizeof(bp->vbp_image)) {
			log_warnx("invalid image name");
			return (-1);
		}
		ac--;
		av++;
	}

	optreset = optind = opterr = 1;
	while ((ch = getopt(ac, av, "acds")) != -1) {
		switch (ch) {
		case 'a':
			bp->vbp_howto |= RB_ASKNAME;
			break;
		case 'c':
			bp->vbp_howto |= RB_CONFIG;
			break;
		case 'd':
			bp->vbp_howto |= RB_KDB;
			break;
		case 's':
			bp->vbp_howto |= RB_SINGLE;
			break;
		default:
			log_warnx("invalid boot option: %c", ch);
			return (-1);
		}
	}

	return (0);
}

uint32_t
vmboot_bootdevice(const char *word)
{
	uint32_t	 bootdev = 0;
	int		 disk, part;

	if (strlen(word) != strlen("hd0a")) {
		log_warnx("invalid boot device: %s", word);
		goto done;
	}

	if (strncmp("hd", word, 2) != 0) {
		log_warnx("unsupported boot device type: %s", word);
		goto done;
	}

	disk = (int)word[2];
	part = (int)word[3];

	if (!(isdigit(disk) && isalpha(part) && islower(part))) {
		log_warnx("invalid boot partition: %s", word);
		goto done;
	}

	disk -= '0';
	part -= 'a';

	if (disk != 0 || part > MAXPARTITIONS) {
		log_warnx("cannot boot from device: %s", word);
		goto done;
	}

	bootdev = MAKEBOOTDEV(0x4, 0, 0, disk, part);

 done:
	/* returns 0 on error */
	return (bootdev);
}

int
vmboot_bootconf(char *conf, size_t size, struct vmboot_params *bp)
{
	char	 buf[BUFSIZ];
	FILE	*fp;

	if ((fp = fmemopen(conf, size, "r")) == NULL) {
		log_debug("%s: failed to boot.conf memory stream", __func__);
		return (-1);
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		buf[strcspn(buf, "\n")] = '\0';
		vmboot_bootcmd(buf, bp);
	}
	fclose(fp);

	if (strlen(bp->vbp_device))
		log_debug("%s: set device %s", __func__, bp->vbp_device);
	if (strlen(bp->vbp_image))
		log_debug("%s: set image %s", __func__, bp->vbp_image);
	if (bp->vbp_howto) {
		snprintf(buf, sizeof(buf), "boot -%s%s%s%s",
		    (bp->vbp_howto & RB_ASKNAME) ? "a" : "",
		    (bp->vbp_howto & RB_CONFIG) ? "c" : "",
		    (bp->vbp_howto & RB_KDB) ? "d" : "",
		    (bp->vbp_howto & RB_SINGLE) ? "s" : "");
		log_debug("%s: %s", __func__, buf);
	}

	return (0);
}


/*
 * For ufs.c
 */

struct devsw vmboot_devsw = {
	.dv_name =	"vmboot",
	.dv_strategy =	vmboot_strategy,
	/* other fields are not needed */
};

struct open_file vmboot_file = {
	.f_dev =	&vmboot_devsw,
	.f_devdata =	 NULL
};

int
vmboot_strategy(void *devdata, int rw,
    daddr32_t blk, size_t size, void *buf, size_t *rsize)
{
	struct vmboot_params	*vmboot = devdata;
	struct virtio_backing	*vfp = vmboot->vbp_arg;
	ssize_t			 rlen;

	if (vfp == NULL)
		return (EIO);

	switch (rw) {
	case F_READ:
		rlen = vfp->pread(vfp->p, buf, size,
		    (blk + vmboot->vbp_partoff) * DEV_BSIZE);
		if (rlen == -1)
			return (errno);
		*rsize = (size_t)rlen;
		break;
	case F_WRITE:
		rlen = vfp->pwrite(vfp->p, buf, size,
		    (blk + vmboot->vbp_partoff) * DEV_BSIZE);
		if (rlen == -1)
			return (errno);
		*rsize = (size_t)rlen;
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

/*
 * Based on findopenbsd() from biosdev.c that was partially written by me.
 */
off_t
vmboot_findopenbsd(struct open_file *f, off_t mbroff, struct disklabel *dl)
{
	struct dos_mbr		 mbr;
	struct dos_partition	*dp;
	off_t			 mbr_eoff = DOSBBSECTOR, nextebr;
	int			 ret, i;
	static int		 maxebr = DOS_MAXEBR;
	size_t			 rsize;
	char			 buf[DEV_BSIZE], *msg;

	if (!maxebr--) {
		log_debug("%s: too many extended partitions", __func__);
		return (-1);
	}

	memset(&mbr, 0, sizeof(mbr));
	ret = (f->f_dev->dv_strategy)(f->f_devdata, F_READ,
	    mbroff, sizeof(mbr), &mbr, &rsize);
	if (ret != 0 || rsize != sizeof(mbr)) {
		log_debug("%s: failed to read MBR", __func__);
		return (-1);
	}

	if (mbr.dmbr_sign != DOSMBR_SIGNATURE) {
		log_debug("%s: bad MBR signature", __func__);
		return (-1);
	}

	/* Search for the first OpenBSD partition */
	nextebr = 0;
	for (i = 0; i < NDOSPART; i++) {
		dp = &mbr.dmbr_parts[i];
		if (!dp->dp_size)
			continue;

		if (dp->dp_typ == DOSPTYP_OPENBSD) {
			if (dp->dp_start > (dp->dp_start + mbroff))
				continue;

			/* Load and parse the disk label */
			ret = (f->f_dev->dv_strategy)(f->f_devdata, F_READ,
			    dp->dp_start + mbroff + DOS_LABELSECTOR,
			    sizeof(buf), buf, &rsize);
			if (ret != 0 || rsize != sizeof(buf)) {
				log_warn("could not load disk label");
				return (-1);
			}
			if ((msg = getdisklabel(buf, dl)) != NULL) {
				log_warnx("%s", msg);
				return (-1);
			}

			return (dp->dp_start + mbroff);
		}

		if (!nextebr && (dp->dp_typ == DOSPTYP_EXTEND ||
		    dp->dp_typ == DOSPTYP_EXTENDL)) {
			nextebr = dp->dp_start + mbr_eoff;
			if (nextebr < dp->dp_start)
				nextebr = -1;
			if (mbr_eoff == DOSBBSECTOR)
				mbr_eoff = dp->dp_start;
		}
	}

	if (nextebr && nextebr != -1) {
		mbroff = nextebr;
		return (vmboot_findopenbsd(f, mbroff, dl));
	}

	return (-1);
}

void *
vmboot_loadfile(struct open_file *f, char *file, size_t *size)
{
	char		*buf = NULL, *p = NULL;
	struct stat	 st;
	size_t		 rsize;
	int		 ret;

	*size = 0;

	if ((ret = ufs_open(file, f)) != 0)
		return (NULL);

	if ((ret = ufs_stat(f, &st)) != 0) {
		log_debug("%s: failed to stat %s", __func__, file);
		goto done;
	}

	if ((buf = calloc(1, roundup(st.st_size, DEV_BSIZE))) == NULL) {
		log_debug("%s: failed to allocate buffer", __func__);
		goto done;
	}

	if ((ret = ufs_read(f, buf, st.st_size, &rsize)) != 0) {
		log_debug("%s: failed to read %s", __func__, file);
		free(buf);
		goto done;
	}

	*size = st.st_size;
	p = buf;
 done:
	ufs_close(f);
	return (p);
}

FILE *
vmboot_open(int kernel_fd, int *disk_fd, int nfd, unsigned int disk_type,
    struct vmboot_params *vmboot)
{
	char			 file[PATH_MAX];
	char			*buf = NULL;
	size_t			 size;
	FILE			*fp = NULL;
	struct disklabel	 dl;
	struct virtio_backing	*vfp;
	off_t			 sz;

	memset(vmboot, 0, sizeof(*vmboot));
	memset(&vfp, 0, sizeof(vfp));
	memset(&dl, 0, sizeof(dl));

	/* First open kernel directly if specified by fd */
	if (kernel_fd != -1)
		return (fdopen(kernel_fd, "r"));

	if (disk_fd == NULL || nfd < 1)
		return (NULL);

	if ((vfp = calloc(1, sizeof(*vfp))) == NULL)
		goto fail;
	vmboot->vbp_type = disk_type;
	vmboot->vbp_arg = vfp;

	switch (vmboot->vbp_type) {
	case VMDF_RAW:
		if (virtio_raw_init(vfp, &sz, disk_fd, nfd) == -1) {
			log_debug("%s: could not open raw disk", __func__);
			goto fail;
		}
		break;
	case VMDF_QCOW2:
		if (virtio_qcow2_init(vfp, &sz, disk_fd, nfd) == -1) {
			log_debug("%s: could not open qcow2 disk", __func__);
			goto fail;
		}
		break;
	}

	vmboot_file.f_devdata = vmboot;

	if ((vmboot->vbp_partoff =
	    vmboot_findopenbsd(&vmboot_file, 0, &dl)) == -1) {
		log_debug("%s: could not find openbsd partition", __func__);
		goto fail;
	}

	/* Set the default kernel boot device and image path */
	strlcpy(vmboot->vbp_device, VM_DEFAULT_DEVICE,
	    sizeof(vmboot->vbp_device));
	strlcpy(vmboot->vbp_image, VM_DEFAULT_KERNEL,
	    sizeof(vmboot->vbp_image));

	/* Try to parse boot.conf to overwrite the default kernel path */
	strlcpy(file, VM_BOOT_CONF, sizeof(file));
	if ((buf = vmboot_loadfile(&vmboot_file, file, &size)) != NULL) {
		if (vmboot_bootconf(buf, size, vmboot) == -1) {
			free(buf);
			goto fail;
		}
		free(buf);
	}

	/* Parse boot device and find partition in disk label */
	if ((vmboot->vbp_bootdev =
	    vmboot_bootdevice(vmboot->vbp_device)) == 0)
		goto fail;
	if (B_PARTITION(vmboot->vbp_bootdev) > dl.d_npartitions) {
		log_debug("%s: invalid boot partition: %s",
		    __func__, vmboot->vbp_device);
		goto fail;
	}
	vmboot->vbp_partoff =
	    dl.d_partitions[B_PARTITION(vmboot->vbp_bootdev)].p_offset;

	/* Load the kernel */
	if ((buf = vmboot_loadfile(&vmboot_file,
	    vmboot->vbp_image, &size)) == NULL) {
		log_debug("%s: failed to open kernel %s:%s", __func__,
		    vmboot->vbp_device, vmboot->vbp_image);
		goto fail;
	}
	vmboot->vbp_buf = buf;

	if ((fp = fmemopen(buf, size, "r")) == NULL) {
		log_debug("%s: failed to open memory stream", __func__);
		free(buf);
		vmboot->vbp_buf = NULL;
	} else {
		log_debug("%s: kernel %s:%s", __func__,
		    vmboot->vbp_device, vmboot->vbp_image);
	}

	return (fp);
  fail:
	vmboot_close(fp, vmboot);
	return (NULL);
}

void
vmboot_close(FILE *fp, struct vmboot_params *vmboot)
{
	struct virtio_backing	*vfp = vmboot->vbp_arg;

	if (fp != NULL)
		fclose(fp);
	if (vfp != NULL)
		vfp->close(vfp->p, 1);
	free(vmboot->vbp_arg);
	free(vmboot->vbp_buf);
}
