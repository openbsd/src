/* Well, here's my linux version of findsuper.
 * I'm sure you coulda done it faster.  :)
 * IMHO there isn't as much interesting data to print in the
 * linux superblock as there is in the SunOS superblock--disk geometry is
 * not there...and linux seems to update the dates in all the superblocks.
 * SunOS doesn't ever touch the backup superblocks after the fs is created,
 * as far as I can tell, so the date is more interesting IMHO and certainly
 * marks which superblocks are backup ones.
 *
 * This still doesn't handle disks >2G.
 *
 * I wanted to add msdos support, but I couldn't make heads or tails
 * of the kernel include files to find anything I could look for in msdos.
 * 
 * Reading every block of a Sun partition is fairly quick.  Doing the
 * same under linux (slower hardware I suppose) just isn't the same.
 * It might be more useful to default to reading the first (second?) block
 * on each cyl; however, if the disk geometry is wrong, this is useless.
 * But ya could still get the cyl size to print the numbers as cyls instead
 * of blocks...
 *
 * run this as (for example)
 *   findsuper /dev/hda
 *   findsuper /dev/hda 437760 1024   (my disk has cyls of 855*512)
 *
 * I suppose the next step is to figgure out a way to determine if
 * the block found is the first superblock somehow, and if so, build
 * a partition table from the superblocks found... but this is still
 * useful as is.
 *
 *		Steve
 * ssd@nevets.oau.org
 * ssd@mae.engr.ucf.edu
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <linux/ext2_fs.h>


main(int argc, char *argv[])
{
	int i;
	int skiprate=512;		/* one sector */
	long sk=0;			/* limited to 2G filesystems!! */
	FILE *f;
	char *s;
	time_t tm;

	struct ext2_super_block ext2;
	/* interesting fields: EXT2_SUPER_MAGIC
	 *      s_blocks_count s_log_block_size s_mtime s_magic s_lastcheck */
  
	if (argc<2) {
		fprintf(stderr,
			"Usage:  findsuper device [skiprate [start]]\n");
		exit(1);
	}
	if (argc>2)
		skiprate=atoi(argv[2]);
	if (skiprate<512) {
		fprintf(stderr,
			"Do you really want to skip less than a sector??\n");
		exit(2);
	}
	if (argc>3)
		sk=atol(argv[3]);
	if (sk<0) {
		fprintf(stderr,"Have to start at 0 or greater,not %ld\n",sk);
		exit(1);
	}
	f=fopen(argv[1],"r");
	if (!f) {
		perror(argv[1]);
		exit(1);
	}
 
	/* Now, go looking for the superblock ! */
	printf("  thisoff     block fs_blk_sz  blksz last_mount\n");
	for (;!feof(f) &&  (i=fseek(f,sk,SEEK_SET))!= -1; sk+=skiprate){
		if (i=fread(&ext2,sizeof(ext2),1, f)!=1) {
			perror("read failed");
		} else if (ext2.s_magic == EXT2_SUPER_MAGIC){
			tm = ext2.s_mtime;
			s=ctime(&tm);
			s[24]=0;
			printf("%9ld %9ld %9ld %5ld %s\n",sk,sk/1024,ext2.s_blocks_count,ext2.s_log_block_size,s);
		}
	}
	printf("Failed on %d at %ld\n", i, sk);
	fclose(f);
}
