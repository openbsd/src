/*	$NetBSD: chg_pid.c,v 1.1.1.1 1995/03/26 07:12:04 leo Exp $	*/

/*
 * Copyright (c) 1995 L. Weppelman
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Leo Weppelman.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *
 * This program changes the partition id field (p_id) in the GEM
 * partition info. NetBSD uses these id-fields to determine the kind
 * of partition. Sensible id's to set are:
 *  NBU : NetBSD User partition
 *  NBR : NetBSD Root partition
 *  NBS : NetBSD Swap partition
 *
 * When NetBSD auto boots, the first 'NBR' partition found when scanning the
 * SCSI-disks becomes the active root partition. The same goes for 'NBS'.
 * Drives are scanned in 'SCSI-id' order.
 */
#include <stdio.h>
#include <osbind.h>

/*
 * Format of GEM root sector
 */
typedef struct gem_part {
	u_char	p_flg;		/* bit 0 is in-use flag			*/
	u_char	p_id[3];	/* id: GEM, BGM, XGM, UNX, MIX		*/
	u_long	p_st;		/* block where partition starts		*/
	u_long	p_size;		/* partition size			*/
} GEM_PART;

#define	NGEM_PARTS	4	/* Max. partition infos in root sector	*/

typedef struct gem_root {
	u_char	    fill[0x1c2];	/* Filler, can be boot code	*/
	u_long	    hd_siz;		/* size of entire volume	*/
	GEM_PART    parts[NGEM_PARTS];	/* see above			*/
	u_long	    bsl_st;		/* start of bad-sector list	*/
	u_long	    bsl_cnt;	  	/* nr. blocks in bad-sector list*/
	u_short	    csum;		/* checksum	correction	*/
} GEM_ROOT;

main(argc, argv)
int	argc;
char	*argv[];
{
	int	driveno = 0;
	int	partno  = 0;
	char	*newname;
	int	c;

	if(argc != 4)
		usage();
	driveno = atoi(argv[1]);
	partno  = atoi(argv[2]);
	newname = argv[3];

	printf("Note: drives start numbering at 0!\n");
	printf("About to change id of partition %d on drive %d to %s\n",
											partno, driveno, newname);
	printf("Are you sure (y/n)? ");
	c = getchar();
	switch(c) {
		case 'y':
		case 'Y':
			if(chg_tosparts(partno, driveno, newname))
				printf("Done\n");
			else printf("Partion number not found\n");
			break;
		default :
			printf("Aborted\n");
	}
}

usage()
{
	printf("Usage: chg_pid <driveno> <partno> <newid>\n");
	exit(1);
}

int chg_tosparts(chg_part, drive, newname)
int	chg_part, drive;
char	*newname;
{
	GEM_ROOT	*g_root;
	GEM_PART	g_local[NGEM_PARTS];
	char		buf[512];
	int		pno  = 1;
	int		i;

	/*
	 * Read root sector
	 */
	if(read_block(buf, 0, drive) == 0) {
		fprintf(stderr, "Cannot read block 0\n");
		exit(1);
	}

	/*
	 * Make local copy of partition info, we may need to re-use
	 * the buffer in case of 'XGM' partitions.
	 */
	g_root  = (GEM_ROOT*)buf;
	bcopy(g_root->parts, g_local, NGEM_PARTS*sizeof(GEM_PART));


	for(i = 0; i < NGEM_PARTS; i++) {
	    if(!(g_local[i].p_flg & 1)) 
		continue;
	    if(!strncmp(g_local[i].p_id, "XGM", 3)) {
		int	j;
		daddr_t	new_root = g_local[i].p_st;

		/*
		 * Loop through extended partition list
		 */
		for(;;) {
		    if(read_block(buf, new_root, drive) == 0) {
			fprintf(stderr, "Cannot read block %d\n", new_root);
			exit(1);
		    }
		    for(j = 0; j < NGEM_PARTS; j++) {
			if(!(g_root->parts[j].p_flg & 1))
			    continue;
			if(!strncmp(g_root->parts[j].p_id, "XGM", 3)) {
			    new_root = g_local[i].p_st + g_root->parts[j].p_st;
			    break;
			}
			else {
			    if(pno == chg_part) {
				change_it(pno,g_root->parts[j].p_id, newname);
				if(write_block(buf, new_root, drive) == 0) {
				    fprintf(stderr, "Cannot write block %d\n",
								new_root);
				    exit(1);
				}
				return(1);
			    }
			    pno++;
			}
		    }
		    if(j == NGEM_PARTS)
			break;
		}
	    }
	    else {
		if(pno == chg_part) {
			/*
			 * Re-read block 0
			 */
			if(read_block(buf, 0, drive) == 0) {
				fprintf(stderr, "Cannot read block 0\n");
				exit(1);
			}
			change_it(pno, g_root->parts[i].p_id, newname);
			set_csum(buf);
			if(write_block(buf, 0, drive) == 0) {
				fprintf(stderr, "Cannot write block 0\n");
				exit(1);
			}
			return(1);
		}
		pno++;
	    }
	}
	return(0);
}

change_it(pno, p_id, newname)
int	pno;
char	*p_id, *newname;
{
	char	s1[4], s2[4];

	strncpy(s1, p_id, 3);
	strncpy(s2, newname, 3);
	s1[3] = s2[3] = '\0';
	printf("Changing partition %d: %s -> %s ...", pno, s1, s2);
	p_id[0] = s2[0]; p_id[1] = s2[1]; p_id[2] = s2[2];
}

read_block(buf, blkno, drive)
void	*buf;
int	blkno;
int	drive;
{
	if(Dmaread(blkno, 1, buf, drive + 8) != 0)
		return(0);
	return(1);
}

write_block(buf, blkno, drive)
void	*buf;
int	blkno;
int	drive;
{
	if(Dmawrite(blkno, 1, buf, drive + 8) != 0)
		return(0);
	return(1);
}

set_csum(buf)
char	*buf;
{
	unsigned short	*p = (unsigned short *)buf;
	unsigned short	csum = 0;
	int		i;

	p[255] = 0;
	for(i = 0; i < 256; i++)
		csum += *p++;
	*--p = (0x1234 - csum) & 0xffff;
}
