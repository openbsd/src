/*	$NetBSD: fat.c,v 1.1.4.1 1996/05/31 18:41:50 jtc Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank
 * Copyright (c) 1995 Martin Husemann
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
 *	This product includes software developed by Martin Husemann
 *	and Wolfgang Solfrank.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef lint
static char rcsid[] = "$NetBSD: fat.c,v 1.1.4.1 1996/05/31 18:41:50 jtc Exp $";
#endif /* not lint */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>

#include "ext.h"

/*
 * Check a cluster number for valid value
 */
static int
checkclnum(boot, fat, cl, next)
	struct bootblock *boot;
	int fat;
	cl_t cl;
	cl_t *next;
{
	if (!boot->Is16BitFat && *next >= (CLUST_RSRVD&0xfff))
		*next |= 0xf000;
	if (*next == CLUST_FREE) {
		boot->NumFree++;
		return FSOK;
	}
	if (*next < CLUST_FIRST
	    || (*next >= boot->NumClusters && *next < CLUST_EOFS)) {
		pwarn("Cluster %d in FAT %d continues with %s cluster number %d\n",
		      cl, fat,
		      *next < CLUST_RSRVD ? "out of range" : "reserved",
		      *next);
		if (ask(0, "Truncate")) {
			*next = CLUST_EOF;
			return FSFATMOD;
		}
		return FSERROR;
	}
	return FSOK;
}

/*
 * Read a FAT and decode it into internal format
 */
int
readfat(fs, boot, no, fp)
	int fs;
	struct bootblock *boot;
	int no;
	struct fatEntry **fp;
{
	struct fatEntry *fat;
	u_char *buffer, *p;
	cl_t cl;
	off_t off;
	int size;
	int ret = FSOK;

	boot->NumFree = 0;
	fat = malloc(sizeof(struct fatEntry) * boot->NumClusters);
	buffer = malloc(boot->FATsecs * boot->BytesPerSec);
	if (fat == NULL || buffer == NULL) {
		perror("No space for FAT");
		if (fat)
			free(fat);
		return FSFATAL;
	}
	
	memset(fat, 0, sizeof(struct fatEntry) * boot->NumClusters);

	off = boot->ResSectors + no * boot->FATsecs;
	off *= boot->BytesPerSec;

	if (lseek(fs, off, SEEK_SET) != off) {
		perror("Unable to read FAT");
		free(buffer);
		free(fat);
		return FSFATAL;
	}
	
	if ((size = read(fs, buffer, boot->FATsecs * boot->BytesPerSec))
	    != boot->FATsecs * boot->BytesPerSec) {
		if (size < 0)
			perror("Unable to read FAT");
		else
			pfatal("Short FAT?");
		free(buffer);
		free(fat);
		return FSFATAL;
	}

	/*
	 * Remember start of FAT to allow keeping it in write_fat.
	 */
	fat[0].length = buffer[0]|(buffer[1] << 8)|(buffer[2] << 16);
	if (boot->Is16BitFat)
		fat[0].length |= buffer[3] << 24;
	if (buffer[1] != 0xff || buffer[2] != 0xff
	    || (boot->Is16BitFat && buffer[3] != 0xff)) {
		char *msg = boot->Is16BitFat
			? "FAT starts with odd byte sequence (%02x%02x%02x%02x)\n"
			: "FAT starts with odd byte sequence (%02x%02x%02x)\n";
		pwarn(msg, buffer[0], buffer[1], buffer[2], buffer[3]);
		if (ask(1, "Correct")) {
			fat[0].length = boot->Media|0xffffff;
			ret |= FSFATMOD;
		}
	}
	p = buffer + (boot->Is16BitFat ? 4 : 3);
	for (cl = CLUST_FIRST; cl < boot->NumClusters;) {
		if (boot->Is16BitFat) {
			fat[cl].next = p[0] + (p[1] << 8);
			ret |= checkclnum(boot, no, cl, &fat[cl].next);
			cl++;
			p += 2;
		} else {
			fat[cl].next = (p[0] + (p[1] << 8)) & 0x0fff;
			ret |= checkclnum(boot, no, cl, &fat[cl].next);
			cl++;
			if (cl >= boot->NumClusters)
				break;
			fat[cl].next = ((p[1] >> 4) + (p[2] << 4)) & 0x0fff;
			ret |= checkclnum(boot, no, cl, &fat[cl].next);
			cl++;
			p += 3;
		}
	}
	
	free(buffer);
	*fp = fat;
	return ret;
}

/*
 * Get type of reserved cluster
 */
char *
rsrvdcltype(cl)
	cl_t cl;
{
	if (cl < CLUST_BAD)
		return "reserved";
	if (cl > CLUST_BAD)
		return "as EOF";
	return "bad";
}

static int
clustdiffer(cl, cp1, cp2, fatnum)
	cl_t cl;
	cl_t *cp1;
	cl_t *cp2;
	int fatnum;
{
	if (*cp1 >= CLUST_RSRVD) {
		if (*cp2 >= CLUST_RSRVD) {
			if ((*cp1 < CLUST_BAD && *cp2 < CLUST_BAD)
			    || (*cp1 > CLUST_BAD && *cp2 > CLUST_BAD)) {
				pwarn("Cluster %d is marked %s with different indicators, ",
				      cl, rsrvdcltype(*cp1));
				if (ask(1, "fix")) {
					*cp2 = *cp1;
					return FSFATMOD;
				}
				return FSFATAL;
			}
			pwarn("Cluster %d is marked %s in FAT 1, %s in FAT %d\n",
			      cl, rsrvdcltype(*cp1), rsrvdcltype(*cp2), fatnum);
			if (ask(0, "use FAT #1's entry")) {
				*cp2 = *cp1;
				return FSFATMOD;
			}
			if (ask(0, "use FAT #%d's entry", fatnum)) {
				*cp1 = *cp2;
				return FSFATMOD;
			}
			return FSFATAL;
		}
		pwarn("Cluster %d is marked %s in FAT 1, but continues with cluster %d in FAT %d\n",
		      cl, rsrvdcltype(*cp1), *cp2, fatnum);
		if (ask(0, "Use continuation from FAT %d", fatnum)) {
			*cp1 = *cp2;
			return FSFATMOD;
		}
		if (ask(0, "Use mark from FAT 1")) {
			*cp2 = *cp1;
			return FSFATMOD;
		}
		return FSFATAL;
	}
	if (*cp2 >= CLUST_RSRVD) {
		pwarn("Cluster %d continues with cluster %d in FAT 1, but is marked %s in FAT %d\n",
		      cl, *cp1, rsrvdcltype(*cp2), fatnum);
		if (ask(0, "Use continuation from FAT 1")) {
			*cp2 = *cp1;
			return FSFATMOD;
		}
		if (ask(0, "Use mark from FAT %d", fatnum)) {
			*cp1 = *cp2;
			return FSFATMOD;
		}
		return FSERROR;
	}
	pwarn("Cluster %d continues with cluster %d in FAT 1, but with cluster %d in FAT %d\n",
	      cl, *cp1, *cp2, fatnum);
	if (ask(0, "Use continuation from FAT 1")) {
		*cp2 = *cp1;
		return FSFATMOD;
	}
	if (ask(0, "Use continuation from FAT %d", fatnum)) {
		*cp1 = *cp2;
		return FSFATMOD;
	}
	return FSERROR;
}

/*
 * Compare two FAT copies in memory. Resolve any conflicts and merge them
 * into the first one.
 */
int
comparefat(boot, first, second, fatnum)
	struct bootblock *boot;
	struct fatEntry *first;
	struct fatEntry *second;
	int fatnum;
{
	cl_t cl;
	int ret = FSOK;

	if (first[0].next != second[0].next) {
		pwarn("Media bytes in cluster 1(%02x) and %d(%02x) differ\n",
		      first[0].next, fatnum, second[0].next);
		if (ask(1, "Use media byte from FAT 1")) {
			second[0].next = first[0].next;
			ret |= FSFATMOD;
		} else if (ask(0, "Use media byte from FAT %d", fatnum)) {
			first[0].next = second[0].next;
			ret |= FSFATMOD;
		} else
			ret |= FSERROR;
	}
	for (cl = CLUST_FIRST; cl < boot->NumClusters; cl++)
		if (first[cl].next != second[cl].next)
			ret |= clustdiffer(cl, &first[cl].next, &second[cl].next, fatnum);
	return ret;
}

void
clearchain(boot, fat, head)
	struct bootblock *boot;
	struct fatEntry *fat;
	cl_t head;
{
	cl_t p, q;

	for (p = head; p >= CLUST_FIRST && p < boot->NumClusters; p = q) {
		if (fat[p].head != head)
			break;
		q = fat[p].next;
		fat[p].next = fat[p].head = CLUST_FREE;
		fat[p].length = 0;
	}
}

/*
 * Check a complete FAT in-memory for crosslinks
 */
int
checkfat(boot, fat)
	struct bootblock *boot;
	struct fatEntry *fat;
{
	cl_t head, p, h;
	u_int len;
	int ret = 0;
	int conf;
	
	/*
	 * pass 1: figure out the cluster chains.
	 */
	for (head = CLUST_FIRST; head < boot->NumClusters; head++) {
		/* find next untraveled chain */		
		if (fat[head].head != 0		/* cluster already belongs to some chain*/
		    || fat[head].next == CLUST_FREE)
			continue;		/* skip it. */

		/* follow the chain and mark all clusters on the way */
		for (len = 0, p = head;
		     p >= CLUST_FIRST && p < boot->NumClusters;
		     p = fat[p].next) {
			fat[p].head = head;
			len++;
		}

		/* the head record gets the length */
		fat[head].length = len;
	}
	
	/*
	 * pass 2: check for crosslinked chains (we couldn't do this in pass 1 because
	 * we didn't know the real start of the chain then - would have treated partial
	 * chains as interlinked with their main chain)
	 */
	for (head = CLUST_FIRST; head < boot->NumClusters; head++) {
		/* find next untraveled chain */		
		if (fat[head].head != head)
			continue;

		/* follow the chain to its end (hopefully) */
		for (p = head;
		     fat[p].next >= CLUST_FIRST && fat[p].next < boot->NumClusters;
		     p = fat[p].next)
			if (fat[fat[p].next].head != head)
				break;
		if (fat[p].next >= CLUST_EOFS)
			continue;
		
		if (fat[p].next == 0) {
			pwarn("Cluster chain starting at %d ends with free cluster\n", head);
			if (ask(0, "Clear chain starting at %d", head)) {
				clearchain(boot, fat, head);
				ret |= FSFATMOD;
			} else
				ret |= FSERROR;
			continue;
		}
		if (fat[p].next >= CLUST_RSRVD) {
			pwarn("Cluster chain starting at %d ends with cluster marked %s\n",
			      head, rsrvdcltype(fat[p].next));
			if (ask(0, "Clear chain starting at %d", head)) {
				clearchain(boot, fat, head);
				ret |= FSFATMOD;
			} else
				ret |= FSERROR;
			continue;
		}
		if (fat[p].next < CLUST_FIRST || fat[p].next >= boot->NumClusters) {
			pwarn("Cluster chain starting at %d ends with cluster out of range (%d)\n",
			      head, fat[p].next);
			if (ask(0, "Clear chain starting at %d", head)) {
				clearchain(boot, fat, head);
				ret |= FSFATMOD;
			} else
				ret |= FSERROR;
		}
		pwarn("Cluster chains starting at %d and %d are linked at cluster %d\n",
		      head, fat[p].head, p);
		conf = FSERROR;
		if (ask(0, "Clear chain starting at %d", head)) {
			clearchain(boot, fat, head);
			conf = FSFATMOD;
		}
		if (ask(0, "Clear chain starting at %d", h = fat[p].head)) {
			if (conf == FSERROR) {
				/*
				 * Transfer the common chain to the one not cleared above.
				 */
				for (; p >= CLUST_FIRST && p < boot->NumClusters;
				     p = fat[p].next) {
					if (h != fat[p].head) {
						/*
						 * Have to reexamine this chain.
						 */
						head--;
						break;
					}
					fat[p].head = head;
				}
			}
			clearchain(boot, fat, h);
			conf |= FSFATMOD;
		}
		ret |= conf;
	}

	return ret;
}

/*
 * Write out FATs encoding them from the internal format
 */
int
writefat(fs, boot, fat)
	int fs;
	struct bootblock *boot;
	struct fatEntry *fat;
{
	u_char *buffer, *p;
	cl_t cl;
	int i;
	u_int32_t fatsz;
	off_t off;
	int ret = FSOK;
	
	buffer = malloc(fatsz = boot->FATsecs * boot->BytesPerSec);
	if (buffer == NULL) {
		perror("No space for FAT");
		return FSFATAL;
	}
	memset(buffer, 0, fatsz);
	boot->NumFree = 0;
	buffer[0] = (u_char)fat[0].length;
	buffer[1] = (u_char)(fat[0].length >> 8);
	if (boot->Is16BitFat)
		buffer[3] = (u_char)(fat[0].length >> 24);
	for (cl = CLUST_FIRST, p = buffer; cl < boot->NumClusters;) {
		if (boot->Is16BitFat) {
			p[0] = (u_char)fat[cl].next;
			if (fat[cl].next == CLUST_FREE)
				boot->NumFree++;
			p[1] = (u_char)(fat[cl++].next >> 8);
			p += 2;
		} else {
			if (fat[cl].next == CLUST_FREE)
				boot->NumFree++;
			if (cl + 1 < boot->NumClusters
			    && fat[cl + 1].next == CLUST_FREE)
				boot->NumFree++;
			p[0] = (u_char)fat[cl].next;
			p[1] = (u_char)((fat[cl].next >> 8) & 0xf)
				|(u_char)(fat[cl+1].next << 4);
			p[2] = (u_char)(fat[cl++].next >> 8);
			p += 3;
		}
	}
	for (i = 0; i < boot->FATs; i++) {
		off = boot->ResSectors + i * boot->FATsecs;
		off *= boot->BytesPerSec;
		if (lseek(fs, off, SEEK_SET) != off
		    || write(fs, buffer, fatsz) != fatsz) {
			perror("Unable to write FAT");
			ret = FSFATAL; /* Return immediately?		XXX */
		}
	}
	free(buffer);
	return ret;
}

/*
 * Check a complete in-memory FAT for lost cluster chains
 */
int
checklost(dosfs, boot, fat)
	int dosfs;
	struct bootblock *boot;
	struct fatEntry *fat;
{
	cl_t head;
	int mod = FSOK;
	
	for (head = CLUST_FIRST; head < boot->NumClusters; head++) {
		/* find next untraveled chain */		
		if (fat[head].head != head
		    || fat[head].next == CLUST_FREE
		    || (fat[head].next >= CLUST_RSRVD
			&& fat[head].next < CLUST_EOFS)
		    || (fat[head].flags & FAT_USED))
			continue;

		pwarn("Lost cluster chain at cluster 0x%04x\n%d Cluster(s) lost\n", 
		      head, fat[head].length);
		mod |= reconnect(dosfs, boot, fat, head);
		if (mod & FSFATAL)
			break;
	}
	finishlf();
	
	return mod;
}
