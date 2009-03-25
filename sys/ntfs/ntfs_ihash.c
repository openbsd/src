/*	$OpenBSD: ntfs_ihash.c,v 1.5 2009/03/25 20:39:47 oga Exp $	*/
/*	$NetBSD: ntfs_ihash.c,v 1.1 2002/12/23 17:38:32 jdolecek Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ufs_ihash.c	8.7 (Berkeley) 5/17/95
 * Id: ntfs_ihash.c,v 1.5 1999/05/12 09:42:58 semenu Exp
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/mount.h>

#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <fs/ntfs/ntfs.h>
#include <fs/ntfs/ntfs_inode.h>
#include <fs/ntfs/ntfs_ihash.h>
#else
#include <ntfs/ntfs.h>
#include <ntfs/ntfs_inode.h>
#include <ntfs/ntfs_ihash.h>
#endif

#ifdef MALLOC_DEFINE
MALLOC_DEFINE(M_NTFSNTHASH, "NTFS nthash", "NTFS ntnode hash tables");
#endif

/*
 * Structures associated with inode cacheing.
 */
static LIST_HEAD(nthashhead, ntnode) *ntfs_nthashtbl;
static u_long	ntfs_nthash;		/* size of hash table - 1 */
#define	NTNOHASH(device, inum)	((minor(device) + (inum)) & ntfs_nthash)
#ifndef NULL_SIMPLELOCKS
static struct simplelock ntfs_nthash_slock;
#endif
struct rwlock ntfs_hashlock = RWLOCK_INITIALIZER("ntfs_nthashlock");

/*
 * Initialize inode hash table.
 */
void
ntfs_nthashinit()
{
	ntfs_nthashtbl = HASHINIT(desiredvnodes, M_NTFSNTHASH, M_WAITOK,
	    &ntfs_nthash);
	simple_lock_init(&ntfs_nthash_slock);
}

#ifdef __NetBSD__
/*
 * Reinitialize inode hash table.
 */

void
ntfs_nthashreinit()
{
	struct ntnode *ip;
	struct nthashhead *oldhash, *hash;
	u_long oldmask, mask, val;
	int i;

	hash = HASHINIT(desiredvnodes, M_NTFSNTHASH, M_WAITOK, &mask);

	simple_lock(&ntfs_nthash_slock);
	oldhash = ntfs_nthashtbl;
	oldmask = ntfs_nthash;
	ntfs_nthashtbl = hash;
	ntfs_nthash = mask;
	for (i = 0; i <= oldmask; i++) {
		while ((ip = LIST_FIRST(&oldhash[i])) != NULL) {
			LIST_REMOVE(ip, i_hash);
			val = NTNOHASH(ip->i_dev, ip->i_number);
			LIST_INSERT_HEAD(&hash[val], ip, i_hash);
		}
	}
	simple_unlock(&ntfs_nthash_slock);
	hashdone(oldhash, M_NTFSNTHASH);
}

/*
 * Free the inode hash table. Called from ntfs_done(), only needed
 * on NetBSD.
 */
void
ntfs_nthashdone()
{
	hashdone(ntfs_nthashtbl, M_NTFSNTHASH);
}
#endif

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, return it, even if it is locked.
 */
struct ntnode *
ntfs_nthashlookup(dev, inum)
	dev_t dev;
	ino_t inum;
{
	struct ntnode *ip;
	struct nthashhead *ipp;

	simple_lock(&ntfs_nthash_slock);
	ipp = &ntfs_nthashtbl[NTNOHASH(dev, inum)];
	LIST_FOREACH(ip, ipp, i_hash) {
		if (inum == ip->i_number && dev == ip->i_dev)
			break;
	}
	simple_unlock(&ntfs_nthash_slock);

	return (ip);
}

/*
 * Insert the ntnode into the hash table.
 */
void
ntfs_nthashins(ip)
	struct ntnode *ip;
{
	struct nthashhead *ipp;

	simple_lock(&ntfs_nthash_slock);
	ipp = &ntfs_nthashtbl[NTNOHASH(ip->i_dev, ip->i_number)];
	LIST_INSERT_HEAD(ipp, ip, i_hash);
	ip->i_flag |= IN_HASHED;
	simple_unlock(&ntfs_nthash_slock);
}

/*
 * Remove the inode from the hash table.
 */
void
ntfs_nthashrem(ip)
	struct ntnode *ip;
{
	simple_lock(&ntfs_nthash_slock);
	if (ip->i_flag & IN_HASHED) {
		ip->i_flag &= ~IN_HASHED;
		LIST_REMOVE(ip, i_hash);
	}
	simple_unlock(&ntfs_nthash_slock);
}
