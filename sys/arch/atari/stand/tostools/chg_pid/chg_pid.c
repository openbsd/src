/*	$NetBSD: chg_pid.c,v 1.2 1996/01/07 22:06:04 leo Exp $	*/

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
 *  NBD : General NetBSD partition
 *  RAW : Partition hidden for GEMDOS
 *
 * When NetBSD auto boots, the first 'NBR' partition found when scanning the
 * SCSI-disks becomes the active root partition. The same goes for 'NBS'.
 * Drives are scanned in 'SCSI-id' order.
 */
#include <sys/types.h>
#include <osbind.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "libtos.h"

#ifndef Dmawrite
#define	Dmawrite DMAwrite
#endif
#ifndef Dmaread
#define	Dmaread	 DMAread
#endif

/*
 * Format of GEM root sector
 */
typedef struct gem_part {
	u_char	p_flg;		/* bit 0 is in-use flag			*/
	char	p_id[3];	/* id: GEM, BGM, XGM, UNX, MIX		*/
	u_long	p_st;		/* block where partition starts		*/
	u_long	p_size;		/* partition size			*/
} GEM_PART;

/*
 * Defines for p_flg
 */
#define	P_VALID		0x01	/* info is valid			*/
#define	P_ACTIVE	0x80	/* partition is active			*/

#define	NGEM_PARTS	4	/* Max. partition infos in root sector	*/

typedef struct gem_root {
	u_char	    fill[0x1c2];	/* Filler, can be boot code	*/
	u_long	    hd_siz;		/* size of entire volume	*/
	GEM_PART    parts[NGEM_PARTS];	/* see above			*/
	u_long	    bsl_st;		/* start of bad-sector list	*/
	u_long	    bsl_cnt;	  	/* nr. blocks in bad-sector list*/
	u_short	    csum;		/* checksum	correction	*/
} GEM_ROOT;

void	help 		PROTO((void));
void	usage		PROTO((void));
int	chg_tosparts	PROTO((int, int, char *));
void	change_it	PROTO((int, GEM_PART *, char *));
int	read_block	PROTO((void *, int, int));
int	write_block	PROTO((void *, int, int));
void	set_csum	PROTO((char *));

const char version[] = "$Revision: 1.1 $";

char	*Progname = NULL;		/* What are we called		*/
int	t_flag    = 0;			/* Test -- don't actually do it	*/
int	v_flag    = 0;			/* show version			*/
int	h_flag    = 0;			/* show help			*/

int
main(argc, argv)
int	argc;
char	*argv[];
{
	/*
	 * Option parsing
	 */
	extern	int	optind;
	extern	char	*optarg;

	int	driveno  = 0;
	int	partno   = 0;
	char	*newname = NULL;
	int	c;

	init_toslib(argv[0]);
	Progname = argv[0];

	while ((c = getopt(argc, argv, "htVwo:")) != EOF) {
		switch (c) {
			case 'h':
				h_flag = 1;
				break;
			case 'o':
				redirect_output(optarg);
				break;
			case 't':
				t_flag = 1;
				break;
			case 'V':
				v_flag = 1;
				break;
			case 'w':
				set_wait_for_key();
				break;
			default:
				usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (h_flag)
		help();

	if (v_flag) {
		eprintf("%s\r\n", version);
		if (argc != 3)
			xexit(0);
	}

	if (argc != 3)
		usage();

	eprintf("Note: >>> Both drive and partition numbers start "
		"at 0! <<<\r\n");
			
	driveno = atoi(argv[0]);
	partno  = atoi(argv[1]);
	newname = argv[2];
	eprintf("About to change id of partition %d on drive %d to %s\r\n",
						partno, driveno, newname);

	if (!t_flag)
		c = key_wait("Are you sure (y/n)? ");
	else c = 'y';
	switch(c) {
		case 'y':
		case 'Y':
			if(chg_tosparts(partno, driveno, newname)) {
				if (!t_flag)
					eprintf("Done\r\n");
				else eprintf("Not Done\r\n");
				xexit(0);
			}
			else eprintf("Partition number not found\r\n");
			break;
		default :
			eprintf("Aborted\r\n");
			xexit(1);
			break;
	}
	xexit(0);
}

int chg_tosparts(chg_part, drive, newname)
int	chg_part, drive;
char	*newname;
{
    GEM_ROOT	*g_root;
    GEM_PART	g_local[NGEM_PARTS];
    char	buf[512];
    int		pno  = 0;
    int		i;

    /*
     * Read root sector
     */
    if (read_block(buf, 0, drive) == 0)
	fatal(-1, "Cannot read block 0\r\n");

    /*
     * Make local copy of partition info, we may need to re-use
     * the buffer in case of 'XGM' partitions.
     */
    g_root  = (GEM_ROOT*)buf;
    bcopy(g_root->parts, g_local, NGEM_PARTS*sizeof(GEM_PART));

    for (i = 0; i < NGEM_PARTS; i++) {
	if (!(g_local[i].p_flg & 1)) 
	    continue;
	if (!strncmp(g_local[i].p_id, "XGM", 3)) {
	    int	j;
	    daddr_t	new_root = g_local[i].p_st;

	    /*
	     * Loop through extended partition list
	     */
	    for(;;) {
		if (read_block(buf, new_root, drive) == 0)
		    fatal(-1, "Cannot read block %d\r\n", new_root);
		for (j = 0; j < NGEM_PARTS; j++) {
		    if (!(g_root->parts[j].p_flg & 1))
			continue;
		    if (!strncmp(g_root->parts[j].p_id, "XGM", 3)) {
			new_root = g_local[i].p_st + g_root->parts[j].p_st;
			break;
		    }
		    else {
			if (pno == chg_part) {
			    change_it(pno, &g_root->parts[j], newname);
			    if (t_flag)
				return(1);
			    if (write_block(buf, new_root, drive) == 0)
				fatal(-1, "Cannot write block %d\r\n",new_root);
			    return(1);
			}
			pno++;
		    }
		}
		if (j == NGEM_PARTS)
		    break;
	    }
	}
	else {
	    if (pno == chg_part) {
		/*
		 * Re-read block 0
		 */
		if (read_block(buf, 0, drive) == 0)
		    fatal(-1, "Cannot read block 0\r\n");
		change_it(pno, &g_root->parts[i], newname);
		if (t_flag)
		    return(1);
		set_csum(buf);
		if (write_block(buf, 0, drive) == 0)
		    fatal(-1, "Cannot write block 0\r\n");
		return(1);
	    }
	    pno++;
	}
    }
    return(0);
}

void change_it(pno, gp, newname)
int		pno;
GEM_PART	*gp;
char		*newname;
{
	char	s1[4], s2[4];

	strncpy(s1, gp->p_id, 3);
	strncpy(s2, newname, 3);
	s1[3] = s2[3] = '\0';
	eprintf("Changing partition %d: %s -> %s ...", pno, s1, s2);
	gp->p_id[0] = s2[0]; gp->p_id[1] = s2[1]; gp->p_id[2] = s2[2];
}

int read_block(buf, blkno, drive)
void	*buf;
int	blkno;
int	drive;
{
	if(Dmaread(blkno, 1, buf, drive + 8) != 0)
		return(0);
	return(1);
}

int write_block(buf, blkno, drive)
void	*buf;
int	blkno;
int	drive;
{
	if(Dmawrite(blkno, 1, buf, drive + 8) != 0)
		return(0);
	return(1);
}

void set_csum(buf)
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

void usage()
{
	eprintf("Usage: %s [-hVwt] [ -o <output file>] <driveno> <partno> "
		    "<newid>\r\n", Progname);
	xexit(1);
}

void
help()
{
	eprintf("\r
Change partition identifiers\r
\r
Usage: %s [-hVwt] [ -o <output file>] <driveno> <partno> <newid>\r
\r
Description of options:\r
\r
\t-h  What your getting right now.\r
\t-o  Write output to both <output file> and stdout.\r
\t-V  Print program version.\r
\t-w  Wait for a keypress before exiting.\r
\t-t  Test mode. It does everyting except the modifications on disk.\r
\r
The <driveno> and <partno> arguments specify the drive and the partition\r
this program acts on. Both are zero based.\r
The <newid> argument specifies a 3 letter string that will become the new\r
partition-id.\r
Finally note that the actions of %s are reversable.\r
", Progname, Progname);
	xexit(0);
}
