/*	$OpenBSD: adir.c,v 1.1.1.1 1998/09/14 21:52:54 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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
 * Routines for reading an AFS directory
 */

#include "arla_local.h"

RCSID("$KTH: adir.c,v 1.33 1998/07/29 14:38:51 assar Exp $") ;

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

static DirEntry *
getentry (DirPage0 *page0,
	  unsigned short num)
{
     DirPage1 *page;

     page = (DirPage1 *)((char *)page0 +
			 AFSDIR_PAGESIZE * (num / ENTRIESPERPAGE));
     assert (page->header.pg_tag == htons(AFSDIRMAGIC));
     return &page->entry[num % ENTRIESPERPAGE];
}

/*
 *
 */

static unsigned
find_by_name (DirPage0 *page0,
	      const char *name,
	      VenusFid *fid,
	      const VenusFid *dir)
{
    unsigned i;

    i = ntohs(page0->dheader.hash[hashentry (name)]);
    while (i != 0) {
	const DirEntry *entry = getentry (page0, i - 1);

	if (strcmp (entry->name, name) == 0) {
	    fid->Cell = dir->Cell;
	    fid->fid.Volume = dir->fid.Volume;
	    fid->fid.Vnode  = ntohl (entry->fid.Vnode);
	    fid->fid.Unique = ntohl (entry->fid.Unique);
	    return i;
	}
	i = ntohs(entry->next);
    }
    return i;
}


/*
 * Lookup `name' in the AFS directory identified by `dir' and return
 * the Fid in `file'.  All operations are done as `cred' and return
 * value is 0 or error code.
 */

int
adir_lookup (VenusFid dir,
	     const char *name,
	     VenusFid *file,
	     CredCacheEntry *ce)
{
     int fd;
     DirPage0 *page0;
     FCacheEntry *centry;
     unsigned ind;
     unsigned len;
     int ret;
     fbuf the_fbuf;
     struct stat sb;

     ret = fcache_get (&centry, dir, ce);
     if (ret) {
	 ReleaseWriteLock (&centry->lock);
	 return ret;
     }

     ret = fcache_get_data (centry, ce);
     if (ret) {
	 ReleaseWriteLock (&centry->lock);
	 return ret;
     }

     if (centry->status.FileType != TYPE_DIR) {
	 ReleaseWriteLock (&centry->lock);
	 return ENOTDIR;
     }

#if 0
     len = centry->status.Length;
#endif

     fd = fcache_open_file (centry, O_RDONLY, 0);
     if (fd < 0) {
	 ReleaseWriteLock (&centry->lock);
	 return errno;
     }

     if (fstat (fd, &sb)) {
	 close (fd);
	 ReleaseWriteLock (&centry->lock);
	 return errno;
     }

     len = sb.st_size;

     ret = fbuf_create (&the_fbuf, fd, len, FBUF_READ);
     if (ret) {
	 close (fd);
	 ReleaseWriteLock (&centry->lock);
	 return ret;
     }

     page0 = (DirPage0 *)(the_fbuf.buf);
     ind = find_by_name (page0, name, file, &dir);

     fbuf_end (&the_fbuf);
     ReleaseWriteLock (&centry->lock);
     if (ind != 0)
	  return 0;
     else
	  return ENOENT;
}

/*
 * Read all entries in the AFS directory identified by `dir' and call
 * `func' on each entry with the fid, the name, and `arg'.
 */

int
adir_readdir (VenusFid dir,
	      void (*func)(VenusFid *, const char *, void *), 
	      void *arg,
	      CredCacheEntry *ce)
{
     int fd;
     fbuf the_fbuf;
     DirPage0 *page0;
     unsigned i;
     FCacheEntry *centry;
     int ret;
     unsigned ind, dotind, dotdotind;
     VenusFid fid;
     unsigned len;
     struct stat sb;

     ret = fcache_get (&centry, dir, ce);
     if (ret)
	 return ret;

     ret = fcache_get_data (centry, ce);
     if (ret) {
	 ReleaseWriteLock (&centry->lock);
	 return ret;
     }

     if (centry->status.FileType != TYPE_DIR) {
	 ReleaseWriteLock (&centry->lock);
	 return ENOTDIR;
     }

     fd = fcache_open_file (centry, O_RDONLY, 0);
     if (fd < 0) {
	 ReleaseWriteLock (&centry->lock);
	 return errno;
     }

     if (fstat (fd, &sb)) {
	 ReleaseWriteLock (&centry->lock);
	 close (fd);
	 return errno;
     }

     len = sb.st_size;

     ret = fbuf_create (&the_fbuf, fd, len, FBUF_READ);
     if (ret) {
	 ReleaseWriteLock (&centry->lock);
	 close (fd);
	 return ret;
     }
     page0 = (DirPage0 *)(the_fbuf.buf);

     /*
      * Begin with placing `.' and `..' first. (some system seem to need that)
      */

     dotind = find_by_name (page0, ".", &fid, &dir);
     assert (dotind != 0);
     (*func)(&fid, ".", arg);

     dotdotind = find_by_name (page0, "..", &fid, &dir);
     assert (dotind != 0);
     (*func)(&fid, "..", arg);

     for (i = 0; i < ADIRHASHSIZE; ++i) {
	  const DirEntry *entry;

	  for(ind = ntohs(page0->dheader.hash[i]);
	      ind;
	      ind = ntohs(entry->next)) {

	      entry = getentry (page0, ind - 1);
	      
	      fid.Cell = dir.Cell;
	      fid.fid.Volume = dir.fid.Volume;
	      fid.fid.Vnode  = ntohl (entry->fid.Vnode);
	      fid.fid.Unique = ntohl (entry->fid.Unique);
	      if (ind != dotind && ind != dotdotind) /* already did them */
		  (*func)(&fid, entry->name, arg);
	  }
     }
     fbuf_end (&the_fbuf);
     ReleaseWriteLock (&centry->lock);
     return 0;
}

/*
 *
 */

static int
used_slot (DirPage1 *page, int off)
{
    return page->header.pg_bitmap[off / 8] & (1 << (off % 8));
}

/*
 *
 */

static void
set_used (DirPage1 *page, int off)
{
    page->header.pg_bitmap[off / 8] |= 1 << (off % 8);
}

/*
 * Add a new page to a directory.
 */

static int
create_new_page (DirPage1 **ret_page,
		 fbuf *the_fbuf)
{
    int ret;
    DirPage1 *page1;
    size_t len = the_fbuf->len;

    ret = fbuf_truncate (the_fbuf, len + AFSDIR_PAGESIZE);
    if (ret)
	return ret;

    page1 = (DirPage1 *)((char *)(the_fbuf->buf) + len);
    page1->header.pg_pgcount   = htons(0);
    page1->header.pg_tag       = htons(AFSDIRMAGIC);
    page1->header.pg_freecount = ENTRIESPERPAGE - 1;
    memset (page1->header.pg_bitmap, 0, sizeof(page1->header.pg_bitmap));
    set_used (page1, 0);
    *ret_page = page1;

    return 0;
}

/*
 * return index into `page'
 */

static int
add_to_page (DirPage0 *page0,
	     DirPage1 *page,
	     unsigned pageno,
	     const char *filename,
	     AFSFid fid,
	     unsigned next)
{
    int len = strlen (filename);
    int i, j;
    unsigned n = 1;

    for (len = strlen(filename), n = 1;
	 len > 15;
	 len -= sizeof(DirEntry), ++n)
	;

    if (page0->dheader.map[pageno] < n)
	return -1;

    for (i = 0; i < ENTRIESPERPAGE - n;) {
	for (j = 0; j < n && !used_slot (page, i + j + 1); ++j)
	    ;
	if (j == n) {
	    int k;

	    for (k = i + 1; k < i + j + 1; ++k)
		page->header.pg_bitmap[k / 8] |= (1 << (k % 8));

	    page->entry[i].flag = 0;
	    page->entry[i].length = 0;
	    page->entry[i].next = next;
	    page->entry[i].fid.Vnode  = htonl(fid.Vnode);
	    page->entry[i].fid.Unique = htonl(fid.Unique);
	    strcpy (page->entry[i].name, filename);
	    memset(page->entry[i + j - 1].fill, 0, 4);
	    page0->dheader.map[pageno] -= n;
	    return i;
	}
	i += j + 1;
    }
    return -1;
}

/*
 *
 */

static int
remove_from_page (DirPage0 *page0,
		  DirPage1 *page,
		  unsigned pageno,
		  unsigned off)
{
    DirEntry *entry = &page->entry[off];
    int len;
    unsigned n = 1, i;

    for (len = strlen(entry->name), n = 1;
	 len > 15;
	 len -= sizeof(DirEntry), ++n)
	;

    page0->dheader.map[pageno] += n;

    entry->next = 0;
    entry->fid.Vnode  = 0;
    entry->fid.Unique = 0;

    for (i = off + 1; i < off + n + 1; ++i)
	page->header.pg_bitmap[i / 8] &= ~(1 << (i % 8));
    return 0;
}

/*
 * Create a new directory with only . and ..
 */

int
adir_mkdir (FCacheEntry *dir,
	    AFSFid dot,
	    AFSFid dot_dot)
{
    fbuf the_fbuf;
    int ret;
    int fd;
    DirPage0 *page0;
    DirPage1 *page;
    int ind;
    int i;
    int tmp;

    assert (CheckLock(&dir->lock) == -1);

    fd = fcache_open_file (dir, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
	return errno;

    ret = fbuf_create (&the_fbuf, fd, 0, FBUF_WRITE);
    if (ret)
	return ret;

    ret = create_new_page (&page, &the_fbuf);
    if (ret)
	goto out;

    page0 = (DirPage0 *)(the_fbuf.buf);
    memset (&page0->dheader, 0, sizeof(page0->dheader));
    tmp = ENTRIESPERPAGE
	- (sizeof(PageHeader) + sizeof(DirHeader)) / sizeof(DirEntry);
    page0->header.pg_freecount = tmp;
    page0->dheader.map[0]      = tmp;
    page0->header.pg_pgcount   = htons(1);

    for (i = 0; i < 13; ++i)
	set_used (page, i);

    assert (page0->dheader.hash[hashentry(".")] == 0);

    ind = add_to_page (page0, page, 0, ".", dot, 0);

    assert (ind >= 0);

    page0->dheader.hash[hashentry(".")] = htons(ind + 1);

    assert (page0->dheader.hash[hashentry("..")] == 0);

    ind = add_to_page (page0, page, 0, "..", dot_dot, 0);

    assert (ind >= 0);

    page0->dheader.hash[hashentry("..")] = htons(ind + 1);

out:
    assert (dir->status.Length == the_fbuf.len);
    fbuf_end (&the_fbuf);
    return ret;
}

/*
 * Create a new entry with name `filename' and contents `fid' in `dir.
 */

int
adir_creat (FCacheEntry *dir,
	    const char *name,
	    AFSFid fid)
{
    fbuf the_fbuf;
    int ret;
    int fd;
    int i;
    size_t len;
    unsigned npages;
    DirPage0 *page0;
    DirPage1 *page;
    int ind = 0;
    unsigned hash_value, next;
    struct stat statbuf;

    assert (CheckLock(&dir->lock) == -1);

    assert (dir->flags.datap);

    fd = fcache_open_file (dir, O_RDWR, 0);
    if (fd < 0)
	return errno;

    if(fstat (fd, &statbuf) < 0) {
	close (fd);
	return errno;
    }

    len = statbuf.st_size;

    ret = fbuf_create (&the_fbuf, fd, len, FBUF_WRITE);
    if (ret)
	return ret;

    page0 = (DirPage0 *)(the_fbuf.buf);
    npages = ntohs(page0->header.pg_pgcount);

    assert (npages == the_fbuf.len / AFSDIR_PAGESIZE);

    hash_value = hashentry (name);
    next = page0->dheader.hash[hash_value];

    for (i = 0; i < npages; ++i)
	if (page0->dheader.map[i]) {
	    page = (DirPage1 *)((char *)page0 + i * AFSDIR_PAGESIZE);
	    ind = add_to_page (page0, page, i, name, fid, next);
	    if (ind >= 0)
		break;
	}
    if (i == npages) {
	ret = create_new_page (&page, &the_fbuf);
	if (ret)
	    goto out;
	page0 = (DirPage0 *)(the_fbuf.buf);
	page0->header.pg_pgcount = htons(npages + 1);
	page0->dheader.map[i] = ENTRIESPERPAGE - 1;
	ind = add_to_page (page0, page, i, name, fid, next);
	assert (ind >= 0);
    }
    ind += i * ENTRIESPERPAGE;

    page0->dheader.hash[hash_value] = htons(ind + 1);
    
out:
    /* assert (dir->status.Length == the_fbuf.len); */
    fbuf_end (&the_fbuf);
    return ret;
}

/*
 * Remove the entry named `name' in dir.
 */

int
adir_remove (FCacheEntry *dir,
	     const char *name)
{
    fbuf the_fbuf;
    int ret;
    int fd;
    int i;
    unsigned len;
    DirPage0 *page0;
    DirPage1 *page;
    unsigned hash_value;
    DirEntry *entry = NULL;
    DirEntry *prev_entry;
    unsigned pageno;
    int found;
    struct stat sb;
    unsigned npages;

    assert (CheckLock(&dir->lock) == -1);

    assert (dir->flags.datap);

    fd = fcache_open_file (dir, O_RDWR, 0);
    if (fd < 0)
	return errno;

#if 0
    len = dir->status.Length;
#endif
    if (fstat (fd, &sb)) {
	close (fd);
	return errno;
    }

    len = sb.st_size;

    ret = fbuf_create (&the_fbuf, fd, len, FBUF_WRITE);
    if (ret) {
	close (fd);
	return ret;
    }

    page0 = (DirPage0 *)(the_fbuf.buf);
    npages = ntohs(page0->header.pg_pgcount);
    hash_value = hashentry (name);
    i = ntohs(page0->dheader.hash[hash_value]);
    found = i == 0;
    prev_entry = NULL;
    while (!found) {
	entry = getentry (page0, i - 1);

	if (strcmp (entry->name, name) == 0) {
	    found = TRUE;
	} else {
	    i = ntohs(entry->next);
	    if (i == 0)
		found = TRUE;
	    prev_entry = entry;
	}
    }
    if (i == 0) {
	fbuf_end (&the_fbuf);
	return ENOENT;
    } else {
	if (prev_entry == NULL)
	    page0->dheader.hash[hash_value] = entry->next;
	else
	    prev_entry->next = entry->next;

	pageno = (i - 1) / ENTRIESPERPAGE;
	page = (DirPage1 *)((char *)page0 + pageno * AFSDIR_PAGESIZE);
	remove_from_page (page0, page, pageno, (i - 1) % ENTRIESPERPAGE);
	if (pageno == npages - 1
	    && page0->dheader.map[pageno] == ENTRIESPERPAGE - 1) {
	    do {
		len -= AFSDIR_PAGESIZE;
		--pageno;
		--npages;
	    } while(page0->dheader.map[pageno] == ENTRIESPERPAGE - 1);
	    page0->header.pg_pgcount = htons(npages);
	    fbuf_truncate (&the_fbuf, len);
	}
	fbuf_end (&the_fbuf);
	return 0;
    }
}
