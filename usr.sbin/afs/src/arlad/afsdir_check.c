/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Check a directory in afs format.
 */

#include "arla_local.h"
#include <getarg.h>

RCSID("$arla: afsdir_check.c,v 1.25 2002/07/24 05:57:41 lha Exp $");

static int help_flag;
static int verbose = 0;

/*
 * Hash the filename of one entry.
 */

static unsigned
hashentry (const char *entry)
{
     int s = 0, h;

     while (*entry)
	  s = s * 173 + *entry++;
     h = s & (ADIRHASHSIZE - 1);
     if (h == 0)
	  return h;
     else if( s < 0 )
	  h = ADIRHASHSIZE - h;
     return h;
}

/*
 * Return the entry in the directory given the number.
 * The directory must be contiounsly in memory after page0.
 */

static const DirEntry *
getentry (DirPage0 *page0,
	  unsigned short num)
{
     DirPage1 *page;

     page = (DirPage1 *)((char *)page0 +
			 AFSDIR_PAGESIZE * (num / ENTRIESPERPAGE));
     if (page->header.pg_tag != htons(AFSDIRMAGIC)) {
	 printf ("Bad magic (%d) != %d in directory\n",
		 ntohs(page->header.pg_tag), AFSDIRMAGIC);
	 exit (1);
     }
     return &page->entry[num % ENTRIESPERPAGE];
}

static int
check_dir (const char *filename)
{
    struct stat statbuf;
    int fd;
    fbuf the_fbuf;
    DirPage0 *page0;
    unsigned i, j;
    unsigned ind;
    unsigned len;
    int ret = 0;
    unsigned npages = 0;
    unsigned page_entry_count;
    unsigned hash_entry_count;
    unsigned noverfill;
    uint8_t **my_bitmaps = NULL;

    fd = open (filename, O_RDONLY | O_BINARY, 0);
    if (fd < 0)
	err (1, "open %s", filename);

    if (fstat (fd, &statbuf) < 0)
	err (1, "stat %s", filename);

    len = statbuf.st_size;

    if (len == 0)
	errx (1, "length == 0");

    if (len % AFSDIR_PAGESIZE != 0)
	errx (1, "length %% AFSDIR_PAGESIZE != 0");

    ret = fbuf_create (&the_fbuf, fd, len, FBUF_READ|FBUF_PRIVATE);
    if (ret)
	err (1, "fbuf_create");

    page0 = (DirPage0 *)(the_fbuf.buf);

    printf ("size = %u, pages = %u, pgcount = %u\n",
	    len, len / AFSDIR_PAGESIZE,
	    ntohs(page0->header.pg_pgcount));

    if (len / AFSDIR_PAGESIZE != ntohs(page0->header.pg_pgcount)) {
	ret = 1;
	goto out;
    }

    npages = len / AFSDIR_PAGESIZE;

    my_bitmaps = malloc (npages * sizeof(*my_bitmaps));
    if (my_bitmaps == NULL)
	err (1, "malloc %lu", (unsigned long)npages * sizeof(*my_bitmaps));

    printf ("map: ");
    for (i = 0; i < min(npages, MAXPAGES); ++i) {
	printf ("%u ", page0->dheader.map[i]);
    }
    printf ("\n");

    page_entry_count = 0;

    for (i = 0; i < npages; ++i) {
	PageHeader *ph = (PageHeader *)((char *)page0 + i * AFSDIR_PAGESIZE);
	int start;
	size_t sz = ENTRIESPERPAGE / 8 * sizeof(*my_bitmaps[i]);

	my_bitmaps[i] = malloc (sz);
	if (my_bitmaps[i] == NULL)
	    err (1, "malloc %lu", (unsigned long)sz);

	memset (my_bitmaps[i], 0, sz);

	if (ph->pg_tag != htons(AFSDIRMAGIC)) {
	    printf ("page %d: wrong tag: %u\n", i, htons(ph->pg_tag));
	    ret = 1;
	    goto out;
	}
	printf ("page %d: count = %u, tag = %u, freecount = %u\n",
		i, ntohs(ph->pg_pgcount), htons(ph->pg_tag), ph->pg_freecount);
	if (i == 0) {
	    if (ph->pg_freecount != 51) {
		printf ("freecount should be 51!\n");
		ret = 1;
		goto out;
	    }
	    if (ntohs(ph->pg_pgcount) != npages) {
		printf ("pgcount should be %u!\n", npages);
		ret = 1;
		goto out;
	    }
	} else {
	    if (ph->pg_freecount != 63) {
		printf ("freecount should be 63!\n");
		ret = 1;
		goto out;
	    }
	    if (ntohs(ph->pg_pgcount) != 0) {
		printf ("pgcount should be 0!\n");
		ret = 1;
		goto out;
	    }
	}

	if (i == 0)
	    start = 13;
	else
	    start = 1;

	for (j = start; j < ENTRIESPERPAGE; ++j) {
	    if (ph->pg_bitmap[j / 8] & (1 << (j % 8)))
		++page_entry_count;
	}
    }

    printf ("page entry count = %u\n", page_entry_count);

    hash_entry_count = 0;
    noverfill = 0;

    for (i = 0; i < ADIRHASHSIZE; ++i) {
	const DirEntry *entry;

	for(ind = ntohs(page0->dheader.hash[i]);
	    ind;
	    ind = ntohs(entry->next)) {
	    DirPage1 *page_n;
	    int len;
	    unsigned off;
	    unsigned pageno;

	    entry = getentry (page0, ind - 1);

	    if (verbose)
		printf ("%s - %ld.%ld\n",
			entry->name, 
			(long)ntohl(entry->fid.Vnode),
			(long)ntohl(entry->fid.Unique));
	    
	    if (hashentry (entry->name) != i)
		printf ("wrong name here? hash = %u, name = *%s*\n",
			i, entry->name);

	    pageno = (ind) / ENTRIESPERPAGE;
	    off = (ind) % ENTRIESPERPAGE;

	    page_n = (DirPage1 *)((char *)page0
				  + AFSDIR_PAGESIZE * pageno);

	    if (!(page_n->header.pg_bitmap[off / 8] & (1 << (off % 8)))) {
		printf ("page %d: off %u not set\n",
			(ind - 1) / ENTRIESPERPAGE, off);
	    }

	    my_bitmaps[pageno][off / 8] |= (1 << (off % 8));

	    len = strlen(entry->name);
	    while (len > 15) {
		len -= sizeof(DirEntry);
		++noverfill;
		++off;
		my_bitmaps[pageno][off / 8] |= (1 << (off % 8));
	    }

	    ++hash_entry_count;
	}
    }

    for (i = 0; i < npages; ++i) {
	DirPage1 *page_n;
	int j;
	unsigned unused;

	if (i == 0)
	    unused = 13;
	else
	    unused = 1;

	for (j = 0; j < unused; ++j)
	    my_bitmaps[i][j / 8] |= (1 << (j % 8));

	page_n = (DirPage1 *)((char *)page0 + AFSDIR_PAGESIZE * i);

	if (memcmp (my_bitmaps[i],
		    page_n->header.pg_bitmap, sizeof(my_bitmaps[i])) != 0) {
	    printf ("page %i: bitmaps differ\n"
		    "actual:     ", i);
	    for (j = 0; j < ENTRIESPERPAGE / 8; ++j)
		printf ("%02x ", page_n->header.pg_bitmap[j]);
	    printf ("\n"
		    "calculated: ");
	    for (j = 0; j < ENTRIESPERPAGE / 8; ++j)
		printf ("%02x ", my_bitmaps[i][j]);
	    printf ("\n");
	}
    }


    printf ("hash entry count = %u, noverfill = %u, sum = %u\n",
	    hash_entry_count, noverfill, hash_entry_count + noverfill);

    if (hash_entry_count + noverfill != page_entry_count)
	ret = 1;

out:    
    fbuf_end (&the_fbuf);
    close (fd);
    if (my_bitmaps) {
	for (i = 0; i < npages; ++i)
	    free (my_bitmaps[i]);
	free (my_bitmaps);
    }
    return ret;
}

static struct getargs args[] = {
    {"verbose",	'v',	arg_flag,	&verbose,
     "run in test mode", NULL},
    {"help",	0,	arg_flag,	&help_flag,
     NULL, NULL},
};

static void
usage (int ret)
{
    arg_printusage (args, sizeof(args)/sizeof(*args), NULL, "[file]");
    exit (ret);
}

int
main(int argc, char **argv)
{
    int optind = 0;
    
    if (getarg (args, sizeof(args)/sizeof(*args), argc, argv, &optind))
	usage (1);

    argc -= optind;
    argv += optind;

    if (help_flag)
	usage (0);

    if (argc != 1)
	usage (1);

    return check_dir (argv[0]);
}
