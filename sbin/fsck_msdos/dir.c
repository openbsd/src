/*	$NetBSD: dir.c,v 1.1 1996/05/14 17:39:30 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank
 * Copyright (c) 1995 Martin Husemann
 * Some structure declaration borrowed from Paul Popelka 
 * (paulp@uts.amdahl.com), see /sys/msdosfs/ for reference.
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
static char rcsid[] = "$NetBSD: dir.c,v 1.1 1996/05/14 17:39:30 ws Exp $";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "ext.h"

#define	SLOT_EMPTY	0x00		/* slot has never been used */
#define	SLOT_E5		0x05		/* the real value is 0xe5 */
#define	SLOT_DELETED	0xe5		/* file in this slot deleted */

#define	ATTR_NORMAL	0x00		/* normal file */
#define	ATTR_READONLY	0x01		/* file is readonly */
#define	ATTR_HIDDEN	0x02		/* file is hidden */
#define	ATTR_SYSTEM	0x04		/* file is a system file */
#define	ATTR_VOLUME	0x08		/* entry is a volume label */
#define	ATTR_DIRECTORY	0x10		/* entry is a directory name */
#define	ATTR_ARCHIVE	0x20		/* file is new or modified */

#define	ATTR_WIN95	0x0f		/* long name record */

/*
 * This is the format of the contents of the deTime field in the direntry
 * structure.
 * We don't use bitfields because we don't know how compilers for
 * arbitrary machines will lay them out.
 */
#define DT_2SECONDS_MASK	0x1F	/* seconds divided by 2 */
#define DT_2SECONDS_SHIFT	0
#define DT_MINUTES_MASK		0x7E0	/* minutes */
#define DT_MINUTES_SHIFT	5
#define DT_HOURS_MASK		0xF800	/* hours */
#define DT_HOURS_SHIFT		11

/*
 * This is the format of the contents of the deDate field in the direntry
 * structure.
 */
#define DD_DAY_MASK		0x1F	/* day of month */
#define DD_DAY_SHIFT		0
#define DD_MONTH_MASK		0x1E0	/* month */
#define DD_MONTH_SHIFT		5
#define DD_YEAR_MASK		0xFE00	/* year - 1980 */
#define DD_YEAR_SHIFT		9

/*
 * Calculate a checksum over an 8.3 alias name
 */
static u_char
calcShortSum(p)
	u_char *p;
{
	u_char sum = 0;
	int i;

	for (i = 0; i < 11; i++) {
		sum = (sum << 7)|(sum >> 1);	/* rotate right */
		sum += p[i];
	}

	return sum;
}

/*
 * Global variables temporarily used during a directory scan
 */
static char longName[DOSLONGNAMELEN] = "";
static u_char *buffer = NULL;
static u_char *delbuf = NULL;

/*
 * Init internal state for a new directory scan.
 */
int
resetDosDirSection(boot)
	struct bootblock *boot;
{
	int b1, b2;
	
	b1 = boot->RootDirEnts * 32;
	b2 = boot->SecPerClust * boot->BytesPerSec;
	
	if (!(buffer = malloc(b1 > b2 ? b1 : b2))
	    || !(delbuf = malloc(b2))) {
		perror("No space for directory");
		return FSFATAL;
	}
	return FSOK;
}

/*
 * Cleanup after a directory scan
 */
void
finishDosDirSection()
{
	free(buffer);
	free(delbuf);
	buffer = NULL;
	delbuf = NULL;
}

/*
 * Delete directory entries between startcl, startoff and endcl, endoff.
 */
static int
delete(f, boot, fat, startcl, startoff, endcl, endoff, notlast)
	int f;
	struct bootblock *boot;
	struct fatEntry *fat;
	cl_t startcl;
	int startoff;
	cl_t endcl;
	int endoff;
	int notlast;
{
	u_char *s, *e;
	off_t off;
	int clsz = boot->SecPerClust * boot->BytesPerSec;
	
	s = delbuf + startoff;
	e = delbuf + clsz;
	while (startcl >= CLUST_FIRST && startcl < boot->NumClusters) {
		if (startcl == endcl) {
			if (notlast)
				break;
			e = delbuf + endoff;
		}
		off = startcl * boot->SecPerClust + boot->ClusterOffset;
		off *= boot->BytesPerSec;
		if (lseek(f, off, SEEK_SET) != off
		    || read(f, delbuf, clsz) != clsz) {
			perror("Unable to read directory");
			return FSFATAL;
		}
		while (s < e) {
			*s = SLOT_DELETED;
			s += 32;
		}
		if (lseek(f, off, SEEK_SET) != off
		    || write(f, delbuf, clsz) != clsz) {
			perror("Unable to write directory");
			return FSFATAL;
		}
		if (startcl == endcl)
			break;
		startcl = fat[startcl].next;
		s = delbuf;
	}
	return FSOK;
}

static int
removede(f, boot, fat, start, end, startcl, endcl, curcl, path, eof)
	int f;
	struct bootblock *boot;
	struct fatEntry *fat;
	u_char *start;
	u_char *end;
	cl_t startcl;
	cl_t endcl;
	cl_t curcl;
	char *path;
	int eof;
{
	if (!eof)
		pwarn("Invalid long filename entry for %s\n", path);
	else
		pwarn("Invalid long filename entry at end of directory %s\n", path);
	if (ask(0, "Remove")) {
		if (startcl != curcl) {
			if (delete(f, boot, fat,
				   startcl, start - buffer,
				   endcl, end - buffer,
				   endcl == curcl) == FSFATAL)
				return FSFATAL;
			start = buffer;
		}
		if (endcl == curcl)
			for (; start < end; start += 32)
				*start = SLOT_DELETED;
		return FSDIRMOD;
	}
	return FSERROR;
}
	
/*
 * Check an in-memory file entry
 */
static int
checksize(boot, fat, p, dir)
	struct bootblock *boot;
	struct fatEntry *fat;
	u_char *p;
	struct dosDirEntry *dir;
{
	/*
	 * Check size on ordinary files
	 */
	u_int32_t physicalSize;

	if (dir->head < CLUST_FIRST || dir->head >= boot->NumClusters)
		return FSERROR;
	physicalSize = fat[dir->head].length * boot->ClusterSize;
	if (physicalSize < dir->size) {
		pwarn("size of %s is %lu, should at most be %lu\n",
		      dir->fullpath, dir->size, physicalSize);
		if (ask(1, "Truncate")) {
			dir->size = physicalSize;
			p[28] = (u_char)physicalSize;
			p[29] = (u_char)(physicalSize >> 8);
			p[30] = (u_char)(physicalSize >> 16);
			p[31] = (u_char)(physicalSize >> 24);
			return FSDIRMOD;
		} else
			return FSERROR;
	} else if (physicalSize - dir->size >= boot->ClusterSize) {
		pwarn("%s has too many clusters allocated\n",
		      dir->fullpath);
		if (ask(1, "Drop superfluous clusters")) {
			cl_t cl;
			u_int32_t sz = 0;
			
			for (cl = dir->head; (sz += boot->ClusterSize) < dir->size;)
				cl = fat[cl].next;
			clearchain(boot, fat, fat[cl].next);
			fat[cl].next = CLUST_EOF;
			return FSFATMOD;
		} else
			return FSERROR;
	}
	return FSOK;
}

/*
 * The stack of unread directories
 */
struct dirTodoNode *pendingDirectories = NULL;

/*
 * Read a directory and
 *   - resolve long name records
 *   - enter file and directory records into the parent's list
 *   - push directories onto the todo-stack
 */
int
readDosDirSection(f, boot, fat, dir)
	int f;
	struct bootblock *boot;
	struct fatEntry *fat;
	struct dosDirEntry *dir;
{
	struct dosDirEntry dirent, *d;
	u_char *p, *vallfn, *invlfn, *empty;
	off_t off;
	int i, j, k, last;
	cl_t cl, valcl, invcl, empcl;
	char *t;
	u_int lidx = 0;
	int shortSum;
	int mod = FSOK;
#define	THISMOD	0x8000			/* Only used within this routine */

	cl = dir->head;
	if (dir->fullpath[1] && (cl < CLUST_FIRST || cl >= boot->NumClusters)) {
		/*
		 * Already handled somewhere else.
		 */
		return FSOK;
	}
	shortSum = -1;
	vallfn = invlfn = empty = NULL;
	do {
		if (!dir->fullpath[1]) {
			last = boot->RootDirEnts * 32;
			off = boot->ResSectors + boot->FATs * boot->FATsecs;
		} else {
			last = boot->SecPerClust * boot->BytesPerSec;
			off = cl * boot->SecPerClust + boot->ClusterOffset;
		}
	
		off *= boot->BytesPerSec;
		if (lseek(f, off, SEEK_SET) != off
		    || read(f, buffer, last) != last) {
			perror("Unable to read directory");
			return FSFATAL;
		}
		last /= 32;
		/*
		 * Check `.' and `..' entries here?			XXX
		 */
		for (p = buffer, i = 0; i < last; i++, p += 32) {
			if (dir->fsckflags & DIREMPWARN) {
				*p = SLOT_EMPTY;
				continue;
			}
			
			if (*p == SLOT_EMPTY || *p == SLOT_DELETED) {
				if (*p == SLOT_EMPTY) {
					dir->fsckflags |= DIREMPTY;
					empty = p;
					empcl = cl;
				}
				continue;
			}
			
			if (dir->fsckflags & DIREMPTY) {
				if (!(dir->fsckflags & DIREMPWARN)) {
					pwarn("%s has entries after end of directory\n",
					      dir->fullpath);
					if (ask(1, "Extend")) {
						dir->fsckflags &= ~DIREMPTY;
						if (delete(f, boot, fat,
							   empcl, empty - buffer,
							   cl, p - buffer) == FSFATAL)
							return FSFATAL;
					} else if (ask(0, "Truncate"))
						dir->fsckflags |= DIREMPWARN;
				}
				if (dir->fsckflags & DIREMPWARN) {
					*p = SLOT_DELETED;
					mod |= THISMOD|FSDIRMOD;
					continue;
				} else if (dir->fsckflags & DIREMPTY)
					mod |= FSERROR;
				empty = NULL;
			}
			
			if (p[11] == ATTR_WIN95) {
				if (*p & LRFIRST) {
					if (shortSum != -1) {
						if (!invlfn) {
							invlfn = vallfn;
							invcl = valcl;
						}
					}
					memset(longName, 0, sizeof longName);
					shortSum = p[13];
					vallfn = p;
					valcl = cl;
				} else if (shortSum != p[13]
					   || lidx != *p & LRNOMASK) {
					if (!invlfn) {
						invlfn = vallfn;
						invcl = valcl;
					}
					if (!invlfn) {
						invlfn = p;
						invcl = cl;
					}
					vallfn = NULL;
				}
				lidx = *p & LRNOMASK;
				t = longName + --lidx * 13;
				for (k = 1; k < 11 && t < longName + sizeof(longName); k += 2) {
					if (!p[k] && !p[k + 1])
						break;
					*t++ = p[k];
					/*
					 * Warn about those unusable chars in msdosfs here?	XXX
					 */
					if (p[k + 1])
						t[-1] = '?';
				}
				if (k >= 11)
					for (k = 14; k < 26 && t < longName + sizeof(longName); k += 2) {
						if (!p[k] && !p[k + 1])
							break;
						*t++ = p[k];
						if (p[k + 1])
							t[-1] = '?';
					}
				if (k >= 26)
					for (k = 28; k < 32 && t < longName + sizeof(longName); k += 2) {
						if (!p[k] && !p[k + 1])
							break;
						*t++ = p[k];
						if (p[k + 1])
							t[-1] = '?';
					}
				if (t >= longName + sizeof(longName)) {
					pwarn("long filename too long\n");
					if (!invlfn) {
						invlfn = vallfn;
						invcl = valcl;
					}
					vallfn = NULL;
				}
				if (p[26] | (p[27] << 8)) {
					pwarn("long filename record cluster start != 0\n");
					if (!invlfn) {
						invlfn = vallfn;
						invcl = cl;
					}
					vallfn = NULL;
				}
				continue;	/* long records don't carry further
						 * information */
			}

			/*
			 * This is a standard msdosfs directory entry.
			 */
			memset(&dirent, 0, sizeof dirent);
			
			/*
			 * it's a short name record, but we need to know
			 * more, so get the flags first.
			 */
			dirent.flags = p[11];
			
			/*
			 * Translate from 850 to ISO here		XXX
			 */
			for (j = 0; j < 8; j++)
				dirent.name[j] = p[j];
			dirent.name[8] = '\0';
			for (k = 7; k >= 0 && dirent.name[k] == ' '; k--)
				dirent.name[k] = '\0';
			if (dirent.name[k] != '\0')
				k++;
			if (dirent.name[0] == SLOT_E5)
				dirent.name[0] = 0xe5;
			/*
			 * What about volume names with extensions?	XXX
			 */
			if ((dirent.flags & ATTR_VOLUME) == 0 && p[8] != ' ')
				dirent.name[k++] = '.';
			for (j = 0; j < 3; j++)
				dirent.name[k++] = p[j+8];
			dirent.name[k] = '\0';
			for (k--; k >= 0 && dirent.name[k] == ' '; k--)
				dirent.name[k] = '\0';
			
			if (vallfn && shortSum != calcShortSum(p)) {
				if (!invlfn) {
					invlfn = vallfn;
					invcl = valcl;
				}
				vallfn = NULL;
			}
			dirent.head = p[26] | (p[27] << 8);
			dirent.size = p[28] | (p[29] << 8) | (p[30] << 16) | (p[31] << 24);
			if (vallfn) {
				strcpy(dirent.lname, longName);
				longName[0] = '\0';
				shortSum = -1;
			}
			
			k = strlen(dirent.lname[0] ? dirent.lname : dirent.name);
			k += strlen(dir->fullpath) + 2;
			dirent.fullpath = malloc(k);
			strcpy(dirent.fullpath, dir->fullpath);
			if (dir->fullpath[1])
				strcat(dirent.fullpath, "/");
			strcat(dirent.fullpath,
			       dirent.lname[0] ? dirent.lname : dirent.name);
			if (invlfn) {
				mod |= k = removede(f, boot, fat,
						    invlfn, vallfn ? vallfn : p,
						    invcl, vallfn ? valcl : cl, cl,
						    dirent.fullpath, 0);
				if (mod & FSFATAL)
					return FSFATAL;
				if (vallfn
				    ? (valcl == cl && vallfn != buffer)
				    : p != buffer)
					if (k & FSDIRMOD)
						mod |= THISMOD;
			}
			vallfn = NULL; /* not used any longer */
			invlfn = NULL;
			
			if (dirent.size == 0 && !(dirent.flags & ATTR_DIRECTORY)) {
				if (dirent.head != 0) {
					pwarn("%s has clusters, but size 0\n",
					      dirent.fullpath);
					if (ask(1, "Drop allocated clusters")) {
						p[26] = p[27] = 0;
						clearchain(boot, fat, dirent.head);
						dirent.head = 0;
						mod |= THISMOD|FSDIRMOD|FSFATMOD;
					} else
						mod |= FSERROR;
				}
			} else if (dirent.head == 0
				   && !strcmp(dirent.name, "..")
				   && !strcmp(dir->parent->fullpath, "/")) {
				/*
				 *  Do nothing, the parent is the root
				 */
			} else if (dirent.head < CLUST_FIRST
				   || dirent.head >= boot->NumClusters
				   || fat[dirent.head].next == CLUST_FREE
				   || (fat[dirent.head].next >= CLUST_RSRVD
				       && fat[dirent.head].next < CLUST_EOFS)
				   || fat[dirent.head].head != dirent.head) {
				if (dirent.head == 0)
					pwarn("%s has no clusters\n",
					      dirent.fullpath);
				else if (dirent.head < CLUST_FIRST
					 || dirent.head >= boot->NumClusters)
					pwarn("%s starts with cluster out of range(%d)\n",
					      dirent.fullpath,
					      dirent.head);
				else if (fat[dirent.head].next == CLUST_FREE)
					pwarn("%s starts with free cluster\n",
					      dirent.fullpath);
				else if (fat[dirent.head].next >= CLUST_RSRVD)
					pwarn("%s starts with %s cluster\n",
					      dirent.fullpath,
					      rsrvdcltype(fat[dirent.head].next));
				else
					pwarn("%s doesn't start a cluster chain\n",
					      dirent.fullpath);
				if (dirent.flags & ATTR_DIRECTORY) {
					if (ask(0, "Remove")) {
						*p = SLOT_DELETED;
						mod |= THISMOD|FSDIRMOD;
					} else
						mod |= FSERROR;
					continue;
				} else {
					if (ask(1, "Truncate")) {
						p[28] = p[29] = p[30] = p[31] = 0;
						dirent.size = 0;
						mod |= THISMOD|FSDIRMOD;
					} else
						mod |= FSERROR;
				}
			}
			
			/* create directory tree node */
			d = malloc(sizeof(struct dosDirEntry));
			memcpy(d, &dirent, sizeof(struct dosDirEntry));
			/* link it into the directory tree */
			d->parent = dir;
			d->next = dir->child;
			dir->child = d;
			if (d->head >= CLUST_FIRST && d->head < boot->NumClusters)
				fat[d->head].dirp = d;
			
			if (d->flags & ATTR_DIRECTORY) {
				/*
				 * gather more info for directories
				 */
				struct dirTodoNode * n;
				
				if (d->size) {
					pwarn("Directory %s has size != 0\n",
					      d->fullpath);
					if (ask(1, "Correct")) {
						p[28] = p[29] = p[30] = p[31] = 0;
						d->size = 0;
						mod |= THISMOD|FSDIRMOD;
					} else
						mod |= FSERROR;
				}
				/*
				 * handle `.' and `..' specially
				 */
				if (strcmp(d->name, ".") == 0) {
					if (d->head != dir->head) {
						pwarn("`.' entry in %s has incorrect start cluster\n",
						      dir->fullpath);
						if (ask(1, "Correct")) {
							d->head = dir->head;
							p[26] = (u_char)d->head;
							p[27] = (u_char)(d->head >> 8);
							mod |= THISMOD|FSDIRMOD;
						} else
							mod |= FSERROR;
					}
					continue;
				}
				if (strcmp(d->name, "..") == 0) {
					if (d->head != dir->parent->head) {
						pwarn("`..' entry in %s has incorrect start cluster\n",
						      dir->fullpath);
						if (ask(1, "Correct")) {
							d->head = dir->parent->head;
							p[26] = (u_char)d->head;
							p[27] = (u_char)(d->head >> 8);
							mod |= THISMOD|FSDIRMOD;
						} else
							mod |= FSERROR;
					}
					continue;
				}
				
				boot->NumFiles++;
				/* Enter this directory into the todo list */
				n = malloc(sizeof(struct dirTodoNode));
				n->next = pendingDirectories;
				n->dir = d;
				pendingDirectories = n;
			} else {
				mod |= k = checksize(boot, fat, p, d);
				if (k & FSDIRMOD)
					mod |= THISMOD;
				boot->NumFiles++;
			}
		}
		if (mod & THISMOD) {
			last *= 32;
			if (lseek(f, off, SEEK_SET) != off
			    || write(f, buffer, last) != last) {
				perror("Unable to write directory");
				return FSFATAL;
			}
			mod &= ~THISMOD;
		}
	} while ((cl = fat[cl].next) >= CLUST_FIRST && cl < boot->NumClusters);
	if (invlfn || vallfn)
		mod |= removede(f, boot, fat,
				invlfn ? invlfn : vallfn, p,
				invlfn ? invcl : valcl, -1, 0,
				dir->fullpath, 1);
	return mod & ~THISMOD;
}

/*
 * Try to reconnect a FAT chain into dir
 */
static u_char *lfbuf;
static cl_t lfcl;
static off_t lfoff;

int
reconnect(dosfs, boot, fat, head, dir)
	int dosfs;
	struct bootblock *boot;
	struct fatEntry *fat;
	cl_t head;
	struct dosDirEntry *dir;
{
	struct dosDirEntry d;
	u_char *p;
	
	if (!dir)		/* Create lfdir?			XXX */
		return FSERROR;
	if (!lfbuf) {
		lfbuf = malloc(boot->ClusterSize);
		if (!lfbuf) {
			perror("No space for buffer");
			return FSFATAL;
		}
		p = NULL;
	} else
		p = lfbuf;
	while (1) {
		if (p)
			while (p < lfbuf + boot->ClusterSize)
				if (*p == SLOT_EMPTY
				    || *p == SLOT_DELETED)
					break;
		if (p && p < lfbuf + boot->ClusterSize)
			break;
		lfcl = p ? fat[lfcl].next : dir->head;
		if (lfcl < CLUST_FIRST || lfcl >= boot->NumClusters) {
			/* Extend lfdir?				XXX */
			pwarn("No space in %s\n", LOSTDIR);
			return FSERROR;
		}
		lfoff = lfcl * boot->ClusterSize
		    + boot->ClusterOffset * boot->BytesPerSec;
		if (lseek(dosfs, lfoff, SEEK_SET) != lfoff
		    || read(dosfs, buffer, boot->ClusterSize) != boot->ClusterSize) {
			perror("could not read LOST.DIR");
			return FSFATAL;
		}
		p = lfbuf;
	}

	if (!ask(0, "Reconnect"))
		return FSERROR;

	boot->NumFiles++;
	/* Ensure uniqueness of entry here!				XXX */
	memset(&d, 0, sizeof d);
	sprintf(d.name, "%d", head);
	d.fullpath = malloc(strlen(dir->fullpath) + strlen(d.name) + 2);
	sprintf(d.fullpath, "%s/%s", dir->fullpath, d.name);
	d.flags = 0;
	d.head = head;
	d.size = fat[head].length * boot->ClusterSize;
	d.parent = dir;
	d.next = dir->child;
	dir->child = malloc(sizeof(struct dosDirEntry));
	memcpy(dir->child, &d, sizeof(struct dosDirEntry));
	
	memset(p, 0, 32);
	memset(p, ' ', 11);
	memcpy(p, dir->name, strlen(dir->name));
	p[26] = (u_char)dir->head;
	p[27] = (u_char)(dir->head >> 8);
	p[28] = (u_char)dir->size;
	p[29] = (u_char)(dir->size >> 8);
	p[30] = (u_char)(dir->size >> 16);
	p[31] = (u_char)(dir->size >> 24);
	fat[head].dirp = dir;
	if (lseek(dosfs, lfoff, SEEK_SET) != lfoff
	    || write(dosfs, buffer, boot->ClusterSize) != boot->ClusterSize) {
		perror("could not write LOST.DIR");
		return FSFATAL;
	}
	return FSDIRMOD;
}

void
finishlf()
{
	if (lfbuf)
		free(lfbuf);
	lfbuf = NULL;
}
