/*	$OpenBSD: adir.c,v 1.2 1999/04/30 01:59:06 art Exp $	*/
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

RCSID("$KTH: adir.c,v 1.55 1998/12/25 04:39:55 assar Exp $") ;

/*
 * Hash the filename of one entry.
 */

static unsigned
hashentry (const unsigned char *entry)
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
 * Return the number of additional DirEntries used by an entry with
 * the filename `name`.
 */

static unsigned
additional_entries (const char *filename)
{
    static DirEntry dummy;

    return (strlen(filename) - sizeof(dummy.name) + 1
	    + sizeof(DirEntry) - 1)
	/ sizeof(DirEntry);
}

/*
 * Return a pointer to page number `pageno'.
 */

static DirPage1 *
getpage (DirPage0 *page0, unsigned pageno)
{
    return (DirPage1 *)((char *)page0 + AFSDIR_PAGESIZE * pageno);
}

/*
 * Return the entry `num' in the directory `page0'.
 * The directory must be continuous in memory after page0.
 */

static DirEntry *
getentry (DirPage0 *page0,
	  unsigned short num)
{
     DirPage1 *page = getpage (page0, num / ENTRIESPERPAGE);

     assert (page->header.pg_tag == htons(AFSDIRMAGIC));
     return &page->entry[num % ENTRIESPERPAGE];
}

/*
 * Return a pointer to the entry with name `name' in the directory `page0'.
 */

static DirEntry *
find_entry(DirPage0 *page0, const char *name)
{
    DirEntry *entry;
    unsigned i;

    for (i = ntohs(page0->dheader.hash[hashentry (name)]);
	 i != 0;
	 i = ntohs(entry->next)) {
	entry = getentry (page0, i - 1);

	if (strcmp (entry->name, name) == 0)
	    return entry;
    }
    return NULL;
}

/*
 * Return the fid for the entry with `name' in `page0'.  `fid' is set
 * to the fid of that file.  `dir' should be the fid of the directory.
 * Return zero if succesful, and -1 otherwise.
 */

static int
find_by_name (DirPage0 *page0,
	      const char *name,
	      VenusFid *fid,
	      const VenusFid *dir)
{
    const DirEntry *entry = find_entry (page0, name);

    if (entry == NULL)
	return -1;
    fid->Cell       = dir->Cell;
    fid->fid.Volume = dir->fid.Volume;
    fid->fid.Vnode  = ntohl (entry->fid.Vnode);
    fid->fid.Unique = ntohl (entry->fid.Unique);
    return 0;
}

/*
 * Change the fid for `name' to `fid'.  Return 0 or -1.
 */

static int
update_fid_by_name (DirPage0 *page0,
		    const char *name,
		    const VenusFid *fid,
		    const VenusFid *dir)
{
    DirEntry *entry = find_entry (page0, name);

    if (entry == NULL)
	return -1;

    entry->fid.Vnode = htonl (fid->fid.Vnode);
    entry->fid.Vnode = htonl (fid->fid.Unique);
    return 0;
}

/*
 * Return true if slot `off' on `page' is being used.
 */

static int
used_slot (DirPage1 *page, int off)
{
    return page->header.pg_bitmap[off / 8] & (1 << (off % 8));
}

/*
 * Mark slot `off' on `page' as being used.
 */

static void
set_used (DirPage1 *page, int off)
{
    page->header.pg_bitmap[off / 8] |= 1 << (off % 8);
}

/*
 * Mark slot `off' on `page' as not being used.
 */

static void
set_unused (DirPage1 *page, int off)
{
    page->header.pg_bitmap[off / 8] &= ~(1 << (off % 8));
}

/*
 * Is this page `pageno' empty?
 */

static int
is_page_empty (DirPage0 *page0, unsigned pageno)
{
    DirPage1 *page;
    int i;

    if (pageno < MAXPAGES)
	return page0->dheader.map[pageno] == ENTRIESPERPAGE - 1;
    page = getpage (page0, pageno);
    if (page->header.pg_bitmap[0] != 1)
	return 0;
    for (i = 1; i < sizeof(page->header.pg_bitmap); ++i)
	if (page->header.pg_bitmap[i] != 0)
	    return 0;
    return 1;
}

/*
 * Add a new page to the directory in `the_fbuf', returning a pointer
 * to the new page in `ret_page'.
 * Return 0 iff succesful.
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
 * Create a new entry with name `filename', fid `fid', and next
 * pointer `next' in the page `page' with page number `pageno'.
 * Return the index in the page or -1 if unsuccesful.
 */

static int
add_to_page (DirPage0 *page0,
	     DirPage1 *page,
	     unsigned pageno,
	     const char *filename,
	     AFSFid fid,
	     unsigned next)
{
    int i, j;
    unsigned n;

    n = 1 + additional_entries (filename);

    if (pageno < MAXPAGES && page0->dheader.map[pageno] < n)
	return -1;

    for (i = 0; i < ENTRIESPERPAGE - n;) {
	for (j = 0; j < n && !used_slot (page, i + j + 1); ++j)
	    ;
	if (j == n) {
	    int k;

	    for (k = i + 1; k < i + j + 1; ++k)
		page->header.pg_bitmap[k / 8] |= (1 << (k % 8));

	    page->entry[i].flag = 1;
	    page->entry[i].length = 0;
	    page->entry[i].next = next;
	    page->entry[i].fid.Vnode  = htonl(fid.Vnode);
	    page->entry[i].fid.Unique = htonl(fid.Unique);
	    strcpy (page->entry[i].name, filename);
	    memset(page->entry[i + j - 1].fill, 0, 4);
	    if (pageno < MAXPAGES)
		page0->dheader.map[pageno] -= n;
	    return i;
	}
	i += j + 1;
    }
    return -1;
}

/*
 * Remove the entry `off' from the page `page' (page number `pageno').
 */

static int
remove_from_page (DirPage0 *page0,
		  DirPage1 *page,
		  unsigned pageno,
		  unsigned off)
{
    DirEntry *entry = &page->entry[off];
    unsigned n, i;

    n = 1 + additional_entries (entry->name);

    if (pageno < MAXPAGES)
	page0->dheader.map[pageno] += n;

    entry->next = 0;
    entry->fid.Vnode  = 0;
    entry->fid.Unique = 0;

    for (i = off + 1; i < off + n + 1; ++i)
	set_unused (page, i);
    return 0;
}

/*
 * Lookup `name' in the AFS directory identified by `centry' and return
 * the Fid in `file'.  All operations are done as `cred' and return
 * value is 0 or error code.
 *
 *
 * Locking:
 *            In        Out       Fail
 *    centry: Locked    Locked    Locked
 *  
 */

int
adir_lookup_fcacheentry (FCacheEntry *centry,
			 const char *name,
			 VenusFid *file,
			 CredCacheEntry *ce)
{
     int fd;
     DirPage0 *page0;
     unsigned ind;
     unsigned len;
     int ret;
     int close_ret;
     fbuf the_fbuf;
     struct stat sb;

     assert (CheckLock (&centry->lock) == -1);

     if (centry->status.FileType != TYPE_DIR) {
	 return ENOTDIR;
     }

     assert (CheckLock (&centry->lock) == -1);

     fd = fcache_open_file (centry, O_RDONLY);
     if (fd < 0) {
	 return errno;
     }

     assert (CheckLock (&centry->lock) == -1);

     if (fstat (fd, &sb)) {
	 close_ret = close (fd);
	 assert (close_ret == 0);
	 return errno;
     }

     len = sb.st_size;

     assert (CheckLock (&centry->lock) == -1);

     ret = fbuf_create (&the_fbuf, fd, len, FBUF_READ);
     if (ret) 
	 return ret;

     assert (CheckLock (&centry->lock) == -1);

     page0 = (DirPage0 *)(the_fbuf.buf);
     ind = find_by_name (page0, name, file, &centry->fid);
     fbuf_end (&the_fbuf);

     if (ind == 0)
	 return 0;
     else
	 return ENOENT;
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
    FCacheEntry *centry;
    int ret;

    ret = fcache_get (&centry, dir, ce);
    if (ret)
	return ret;
    
    assert (CheckLock (&centry->lock) == -1);
    
    ret = fcache_get_data (centry, ce);
    if (ret) {
	fcache_release (centry);
	return ret;
    }
    
    ret = adir_lookup_fcacheentry (centry, name, file, ce);

    fcache_release (centry);
    
    return ret;
	
}

/*
 * Lookup `name' in the AFS directory identified by `dir' and change the
 * fid to `fid'.
 */

int
adir_changefid (VenusFid dir,
		const char *name,
		VenusFid *file,
		CredCacheEntry *ce)
{
    int fd;
    DirPage0 *page0;
    FCacheEntry *centry;
    unsigned len;
    int ret;
    int close_ret;
    fbuf the_fbuf;
    struct stat sb;

    ret = fcache_get (&centry, dir, ce);
    if (ret)
	return ret;

    ret = fcache_get_data (centry, ce);
    if (ret) {
	fcache_release (centry);
	return ret;
    }

    if (centry->status.FileType != TYPE_DIR) {
	fcache_release(centry);
	return ENOTDIR;
    }

    fd = fcache_open_file (centry, O_RDWR);
    if (fd < 0) {
	fcache_release(centry);
	return errno;
    }

    if (fstat (fd, &sb)) {
	close_ret = close (fd);
	assert (close_ret == 0);
	fcache_release(centry);
	return errno;
    }

    len = sb.st_size;

    ret = fbuf_create (&the_fbuf, fd, len, FBUF_WRITE);
    if (ret) {
	fcache_release(centry);
	return ret;
    }

    page0 = (DirPage0 *)(the_fbuf.buf);
    ret = update_fid_by_name (page0, name, file, &dir);
    fcache_release(centry);
    fbuf_end (&the_fbuf);

    if (ret == 0)
	return 0;
    else
	return ENOENT;
}

/*
 * Return TRUE if dir is empty.
 */

int
adir_emptyp (VenusFid dir,
	     CredCacheEntry *ce)
{
     int fd;
     DirPage0 *page0;
     FCacheEntry *centry;
     unsigned len;
     int ret;
     int close_ret;
     fbuf the_fbuf;
     struct stat sb;
     unsigned npages;

     ret = fcache_get (&centry, dir, ce);
     if (ret)
	 return ret;

     ret = fcache_get_data (centry, ce);
     if (ret) {
	 fcache_release (centry);
	 return ret;
     }

     if (centry->status.FileType != TYPE_DIR) {
	 fcache_release (centry);
	 return ENOTDIR;
     }

     fd = fcache_open_file (centry, O_RDONLY);
     if (fd < 0) {
	 fcache_release(centry);
	 return errno;
     }

     if (fstat (fd, &sb)) {
	 fcache_release(centry);
	 close_ret = close (fd);
	 assert (close_ret == 0);
	 return errno;
     }

     len = sb.st_size;

     ret = fbuf_create (&the_fbuf, fd, len, FBUF_READ);
     if (ret) {
	 fcache_release(centry);
	 return ret;
     }
     page0 = (DirPage0 *)(the_fbuf.buf);
     npages = ntohs(page0->header.pg_pgcount);

     ret = (npages == 1) && (page0->dheader.map[0] == 2);
     fbuf_end (&the_fbuf);
     fcache_release (centry);
     return ret;
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
     unsigned i, j;
     FCacheEntry *centry;
     int ret;
     int close_ret;
     VenusFid fid;
     unsigned len;
     struct stat sb;
     unsigned npages;

     ret = fcache_get (&centry, dir, ce);
     if (ret)
	 return ret;

     ret = fcache_get_data (centry, ce);
     if (ret) {
	 fcache_release (centry);
	 return ret;
     }

     if (centry->status.FileType != TYPE_DIR) {
	 fcache_release (centry);
	 return ENOTDIR;
     }

     fd = fcache_open_file (centry, O_RDONLY);
     if (fd < 0) {
	 fcache_release(centry);
	 return errno;
     }

     if (fstat (fd, &sb)) {
	 fcache_release(centry);
	 close_ret = close (fd);
	 assert (close_ret == 0);
	 return errno;
     }

     len = sb.st_size;

     ret = fbuf_create (&the_fbuf, fd, len, FBUF_READ);
     if (ret) {
	 fcache_release(centry);
	 return ret;
     }
     page0 = (DirPage0 *)(the_fbuf.buf);
     npages = ntohs(page0->header.pg_pgcount);

     if (npages < len / AFSDIR_PAGESIZE)
	 npages = len / AFSDIR_PAGESIZE;

     for (i = 0; i < npages; ++i) {
	 DirPage1 *page = getpage (page0, i);
	 unsigned first_slot;

	 if (i == 0)
	     first_slot = 12;
	 else
	     first_slot = 0;

	 for (j = first_slot; j < ENTRIESPERPAGE - 1; ++j) {
	     if (used_slot (page, j + 1)) {
		 DirEntry *entry = &page->entry[j];

		 assert (entry->flag);

		 fid.Cell       = dir.Cell;
		 fid.fid.Volume = dir.fid.Volume;
		 fid.fid.Vnode  = ntohl (entry->fid.Vnode);
		 fid.fid.Unique = ntohl (entry->fid.Unique);

		 (*func)(&fid, entry->name, arg);

		 j += additional_entries (entry->name);
	     }
	 }
     }

     fbuf_end (&the_fbuf);
     fcache_release(centry);
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
    int close_ret;

    assert (CheckLock(&dir->lock) == -1);

    fd = fcache_open_file (dir, O_RDWR);
    if (fd < 0)
	return errno;

    if (ftruncate (fd, 0) < 0) {
	close_ret = close (fd);
	assert (close_ret == 0);
	return errno;
    }

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
    fcache_update_length (dir, fbuf_len(&the_fbuf));
    fbuf_end (&the_fbuf);
    return ret;
}

/*
 * Create a new entry with name `filename' and contents `fid' in `dir'.
 */

int
adir_creat (FCacheEntry *dir,
	    const char *name,
	    AFSFid fid)
{
    fbuf the_fbuf;
    int ret;
    int close_ret;
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

    fd = fcache_open_file (dir, O_RDWR);
    if (fd < 0)
	return errno;

    if(fstat (fd, &statbuf) < 0) {
	close_ret = close (fd);
	assert (close_ret == 0);
	return errno;
    }

    len = statbuf.st_size;

    ret = fbuf_create (&the_fbuf, fd, len, FBUF_WRITE);
    if (ret)
	return ret;

    assert (the_fbuf.len != 0);

    page0 = (DirPage0 *)(the_fbuf.buf);
    npages = ntohs(page0->header.pg_pgcount);

    assert (npages == the_fbuf.len / AFSDIR_PAGESIZE);

    hash_value = hashentry (name);
    next = page0->dheader.hash[hash_value];

    for (i = 0; i < npages; ++i) {
	page = getpage (page0, i);
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
	if (i < MAXPAGES)
	    page0->dheader.map[i] = ENTRIESPERPAGE - 1;
	ind = add_to_page (page0, page, i, name, fid, next);
	assert (ind >= 0);
    }
    ind += i * ENTRIESPERPAGE;

    page0->dheader.hash[hash_value] = htons(ind + 1);
    
out:
    fcache_update_length (dir, fbuf_len(&the_fbuf));
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
    int close_ret;
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

    fd = fcache_open_file (dir, O_RDWR);
    if (fd < 0)
	return errno;

    if (fstat (fd, &sb)) {
	close_ret = close (fd);
	assert (close_ret == 0);
	return errno;
    }

    assert (dir->status.Length == sb.st_size);

    len = sb.st_size;

    ret = fbuf_create (&the_fbuf, fd, len, FBUF_WRITE);
    if (ret) {
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
	page = getpage (page0, pageno);
	remove_from_page (page0, page, pageno, (i - 1) % ENTRIESPERPAGE);
	if (pageno == npages - 1
	    && is_page_empty (page0, pageno)) {
	    do {
		len -= AFSDIR_PAGESIZE;
		--pageno;
		--npages;
	    } while(is_page_empty(page0, pageno));
	    page0->header.pg_pgcount = htons(npages);
	    fbuf_truncate (&the_fbuf, len);
	    fcache_update_length (dir, len);
	}
	fbuf_end (&the_fbuf);
	return 0;
    }
}
