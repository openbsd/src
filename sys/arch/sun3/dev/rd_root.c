
#include <sys/param.h>
#include <sys/reboot.h>

#include <dev/ramdisk.h>

extern int boothowto;

#ifndef MINIROOTSIZE
#define MINIROOTSIZE 512
#endif

#define ROOTBYTES (MINIROOTSIZE << DEV_BSHIFT)

/* These two could be patched. */
int rd_root_size = ROOTBYTES;
char rd_root_image[ROOTBYTES] = "|This is the root ramdisk!\n";

/*
 * This is called during autoconfig.
 */
void
rd_attach_hook(unit, rd)
	int unit;
	struct rd_conf *rd;
{
	if (unit == 0) {
		/* Setup root ramdisk */
		rd->rd_addr = (caddr_t) rd_root_image;
		rd->rd_size = (size_t)  rd_root_size;
		rd->rd_type = RD_KMEM_FIXED;
		printf(" fixed, %d blocks", MINIROOTSIZE);
	}
}

/*
 * This is called during open (i.e. mountroot)
 */
void
rd_open_hook(unit, rd)
	int unit;
	struct rd_conf *rd;
{
	if (unit == 0) {
		/* The root ramdisk only works single-user. */
		boothowto |= RB_SINGLE;
	}
}
