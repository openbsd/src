/*	$OpenPackages$ */
/*	$OpenBSD: arch.c,v 1.61 2007/09/16 09:46:14 espie Exp $ */
/*	$NetBSD: arch.c,v 1.17 1996/11/06 17:58:59 christos Exp $	*/

/*
 * Copyright (c) 1999,2000 Marc Espie.
 *
 * Extensive code changes for the OpenBSD project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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
 */

/*
 *	Once again, cacheing/hashing comes into play in the manipulation
 * of archives. The first time an archive is referenced, all of its members'
 * headers are read and hashed and the archive closed again. All hashed
 * archives are kept in a hash (archives) which is searched each time 
 * an archive member is referenced.
 *
 */

#include <sys/param.h>
#include <ar.h>
#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ohash.h"
#include "config.h"
#include "defines.h"
#include "dir.h"
#include "arch.h"
#include "var.h"
#include "targ.h"
#include "memory.h"
#include "gnode.h"
#include "timestamp.h"
#include "lst.h"

#ifndef PATH_MAX
# ifdef MAXPATHLEN
#  define PATH_MAX (MAXPATHLEN+1)
# else
#  define PATH_MAX	1024
# endif
#endif

#ifdef TARGET_MACHINE
#undef MACHINE
#define MACHINE TARGET_MACHINE
#endif
#ifdef TARGET_MACHINE_ARCH
#undef MACHINE_ARCH
#define MACHINE_ARCH TARGET_MACHINE_ARCH
#endif

static struct ohash	  archives;   /* Archives we've already examined.  */

typedef struct Arch_ {
    struct ohash   members;    /* All the members of this archive, as
			       * struct arch_member entries.  */
    char	  name[1];    /* Archive name.	*/
} Arch;

/* Used to get to ar's field sizes.  */
static struct ar_hdr *dummy;
#define AR_NAME_SIZE		(sizeof(dummy->ar_name))
#define AR_DATE_SIZE		(sizeof(dummy->ar_date))

/* Each archive member is tied to an arch_member structure,
 * suitable for hashing.  */
struct arch_member {
    TIMESTAMP	  mtime;	/* Member modification date.  */
    char	  date[AR_DATE_SIZE+1];
				/* Same, before conversion to numeric value.  */
    char	  name[1];	/* Member name.  */
};

static struct ohash_info members_info = {
    offsetof(struct arch_member, name), NULL,
    hash_alloc, hash_free, element_alloc
};

static struct ohash_info arch_info = {
    offsetof(Arch, name), NULL, hash_alloc, hash_free, element_alloc
};



static struct arch_member *new_arch_member(struct ar_hdr *, const char *);
static TIMESTAMP mtime_of_member(struct arch_member *);
static long field2long(const char *, size_t);
static Arch *read_archive(const char *, const char *);

#ifdef CLEANUP
static void ArchFree(Arch *);
#endif
static TIMESTAMP ArchMTimeMember(const char *, const char *, bool);
static FILE *ArchFindMember(const char *, const char *, struct ar_hdr *, const char *);
static void ArchTouch(const char *, const char *);
#if defined(__svr4__) || defined(__SVR4) || \
    (defined(__OpenBSD__) && defined(__ELF__))
#define SVR4ARCHIVES
#endif

#ifdef SVR4ARCHIVES
struct SVR4namelist {
    char	  *fnametab;  /* Extended name table strings */
    size_t	  fnamesize;  /* Size of the string table */
};

static const char *svr4list = "Archive list";

static char *ArchSVR4Entry(struct SVR4namelist *, const char *, size_t, FILE *);
#endif

static struct arch_member *
new_arch_member(struct ar_hdr *hdr, const char *name)
{
    const char *end = NULL;
    struct arch_member *n;

    n = ohash_create_entry(&members_info, name, &end);
    /* XXX ar entries are NOT null terminated.	*/
    memcpy(n->date, &(hdr->ar_date), AR_DATE_SIZE);
    n->date[AR_DATE_SIZE] = '\0';
    /* Don't compute mtime before it is needed. */
    ts_set_out_of_date(n->mtime);
    return n;
}

static TIMESTAMP
mtime_of_member(struct arch_member *m)
{
    if (is_out_of_date(m->mtime))
	ts_set_from_time_t((time_t) strtol(m->date, NULL, 10), m->mtime);
    return m->mtime;
}

#ifdef CLEANUP
/*-
 *-----------------------------------------------------------------------
 * ArchFree --
 *	Free memory used by an archive
 *-----------------------------------------------------------------------
 */
static void
ArchFree(Arch *a)
{
    struct arch_member *mem;
    unsigned int i;

    /* Free memory from hash entries */
    for (mem = ohash_first(&a->members, &i); mem != NULL;
	mem = ohash_next(&a->members, &i))
	free(mem);

    ohash_delete(&a->members);
    free(a);
}
#endif



/* Side-effects: Some nodes may be created.  */
bool
Arch_ParseArchive(char **linePtr,   /* Pointer to start of specification */
    Lst nodeLst, 		    /* Lst on which to place the nodes */
    SymTable *ctxt)		    /* Context in which to expand variables */
{
    char	    *cp;	    /* Pointer into line */
    GNode	    *gn;	    /* New node */
    char	    *libName;	    /* Library-part of specification */
    char	    *memberName;    /* Member-part of specification */
    char	    nameBuf[MAKE_BSIZE]; /* temporary place for node name */
    char	    saveChar;	    /* Ending delimiter of member-name */
    bool	    subLibName;     /* true if libName should have/had
				     * variable substitution performed on it */

    libName = *linePtr;

    subLibName = false;

    for (cp = libName; *cp != '(' && *cp != '\0';) {
	if (*cp == '$') {
	    if (!Var_ParseSkip(&cp, ctxt))
		return false;
	    subLibName = true;
	} else
	    cp++;
    }

    *cp++ = '\0';
    if (subLibName)
	libName = Var_Subst(libName, ctxt, true);

    for (;;) {
	/* First skip to the start of the member's name, mark that
	 * place and skip to the end of it (either white-space or
	 * a close paren).  */
	bool doSubst = false; /* true if need to substitute in memberName */

	while (isspace(*cp))
	    cp++;
	memberName = cp;
	while (*cp != '\0' && *cp != ')' && !isspace(*cp)) {
	    if (*cp == '$') {
		if (!Var_ParseSkip(&cp, ctxt))
		    return false;
		doSubst = true;
	    } else
		cp++;
	}

	/* If the specification ends without a closing parenthesis,
	 * chances are there's something wrong (like a missing backslash),
	 * so it's better to return failure than allow such things to
	 * happen.  */
	if (*cp == '\0') {
	    printf("No closing parenthesis in archive specification\n");
	    return false;
	}

	/* If we didn't move anywhere, we must be done.  */
	if (cp == memberName)
	    break;

	saveChar = *cp;
	*cp = '\0';

	/* XXX: This should be taken care of intelligently by
	 * SuffExpandChildren, both for the archive and the member portions.  */

	/* If member contains variables, try and substitute for them.
	 * This will slow down archive specs with dynamic sources, of course,
	 * since we'll be (non-)substituting them three times, but them's
	 * the breaks -- we need to do this since SuffExpandChildren calls
	 * us, otherwise we could assume the thing would be taken care of
	 * later.  */
	if (doSubst) {
	    char    *buf;
	    char    *sacrifice;
	    char    *oldMemberName = memberName;
	    size_t  length;

	    memberName = Var_Subst(memberName, ctxt, true);

	    /* Now form an archive spec and recurse to deal with nested
	     * variables and multi-word variable values.... The results
	     * are just placed at the end of the nodeLst we're returning.  */
	    length = strlen(memberName)+strlen(libName)+3;
	    buf = sacrifice = emalloc(length);

	    snprintf(buf, length, "%s(%s)", libName, memberName);

	    if (strchr(memberName, '$') && 
	    	strcmp(memberName, oldMemberName) == 0) {
		/* Must contain dynamic sources, so we can't deal with it now.
		 * Just create an ARCHV node for the thing and let
		 * SuffExpandChildren handle it...  */
		gn = Targ_FindNode(buf, TARG_CREATE);

		if (gn == NULL) {
		    free(buf);
		    return false;
		} else {
		    gn->type |= OP_ARCHV;
		    Lst_AtEnd(nodeLst, gn);
		}
	    } else if (!Arch_ParseArchive(&sacrifice, nodeLst, ctxt)) {
		/* Error in nested call -- free buffer and return false
		 * ourselves.  */
		free(buf);
		return false;
	    }
	    /* Free buffer and continue with our work.	*/
	    free(buf);
	} else if (Dir_HasWildcards(memberName)) {
	    LIST  members;
	    char  *member;

	    Lst_Init(&members);

	    Dir_Expand(memberName, dirSearchPath, &members);
	    while ((member = (char *)Lst_DeQueue(&members)) != NULL) {
		snprintf(nameBuf, MAKE_BSIZE, "%s(%s)", libName, member);
		free(member);
		gn = Targ_FindNode(nameBuf, TARG_CREATE);
		/* We've found the node, but have to make sure the rest of
		 * the world knows it's an archive member, without having
		 * to constantly check for parentheses, so we type the
		 * thing with the OP_ARCHV bit before we place it on the
		 * end of the provided list.  */
		gn->type |= OP_ARCHV;
		Lst_AtEnd(nodeLst, gn);
	    }
	} else {
	    snprintf(nameBuf, MAKE_BSIZE, "%s(%s)", libName, memberName);
	    gn = Targ_FindNode(nameBuf, TARG_CREATE);
	    /* We've found the node, but have to make sure the rest of the
	     * world knows it's an archive member, without having to
	     * constantly check for parentheses, so we type the thing with
	     * the OP_ARCHV bit before we place it on the end of the
	     * provided list.  */
	    gn->type |= OP_ARCHV;
	    Lst_AtEnd(nodeLst, gn);
	}
	if (doSubst)
	    free(memberName);

	*cp = saveChar;
    }

    /* If substituted libName, free it now, since we need it no longer.  */
    if (subLibName)
	free(libName);

    /* We promised the pointer would be set up at the next non-space, so
     * we must advance cp there before setting *linePtr... (note that on
     * entrance to the loop, cp is guaranteed to point at a ')') */
    do {
	cp++;
    } while (isspace(*cp));

    *linePtr = cp;
    return true;
}

/* Helper function: ar fields are not null terminated.	*/
static long
field2long(const char *field, size_t length)
{
    static char enough[32];

    assert(length < sizeof(enough));
    memcpy(enough, field, length);
    enough[length] = '\0';
    return strtol(enough, NULL, 10);
}

static Arch *
read_archive(const char *archive, const char *earchive)
{
    FILE *	  arch;       /* Stream to archive */
    char	  magic[SARMAG];
    Arch	  *ar;
#ifdef SVR4ARCHIVES
    struct SVR4namelist list;

    list.fnametab = NULL;
#endif

    /* When we encounter an archive for the first time, we read its
     * whole contents, to place it in the cache.  */
    arch = fopen(archive, "r");
    if (arch == NULL)
	return NULL;

    /* Make sure this is an archive we can handle.  */
    if ((fread(magic, SARMAG, 1, arch) != 1) ||
	(strncmp(magic, ARMAG, SARMAG) != 0)) {
	    fclose(arch);
	    return NULL;
    }

    ar = ohash_create_entry(&arch_info, archive, &earchive);
    ohash_init(&ar->members, 8, &members_info);

    for (;;) {
	size_t		n;
	struct ar_hdr	arHeader;/* Archive-member header for reading archive */
	off_t		size;	/* Size of archive member */
	char		buffer[PATH_MAX];
	char		*memberName;
				/* Current member name while hashing. */
	char		*cp;	/* Useful character pointer */

	memberName = buffer;
	n = fread(&arHeader, 1, sizeof(struct ar_hdr), arch);

	/*  Whole archive read ok.  */
	if (n == 0 && feof(arch)) {
#ifdef SVR4ARCHIVES
	    efree(list.fnametab);
#endif
	    fclose(arch);
	    return ar;
	}
	if (n < sizeof(struct ar_hdr))
	    break;

	if (memcmp(arHeader.ar_fmag, ARFMAG, sizeof(arHeader.ar_fmag)) != 0) {
	    /* The header is bogus.  */
	    break;
	} else {
	    /* We need to advance the stream's pointer to the start of the
	     * next header.  Records are padded with newlines to an even-byte
	     * boundary, so we need to extract the size of the record and
	     * round it up during the seek.  */
	    size = (off_t) field2long(arHeader.ar_size, 
	    	sizeof(arHeader.ar_size));

	    (void)memcpy(memberName, arHeader.ar_name, AR_NAME_SIZE);
	    /* Find real end of name (strip extranous ' ')  */
	    for (cp = memberName + AR_NAME_SIZE - 1; *cp == ' ';)
		cp--;
	    cp[1] = '\0';

#ifdef SVR4ARCHIVES
	    /* SVR4 names are slash terminated.  Also svr4 extended AR format.
	     */
	    if (memberName[0] == '/') {
		/* SVR4 magic mode.  */
		memberName = ArchSVR4Entry(&list, memberName, size, arch);
		if (memberName == NULL)		/* Invalid data */
		    break;
		else if (memberName == svr4list)/* List of files entry */
		    continue;
		/* Got the entry.  */
		/* XXX this assumes further processing, such as AR_EFMT1,
		 * also applies to SVR4ARCHIVES.  */
	    }
	    else {
		if (cp[0] == '/')
		    cp[0] = '\0';
	    }
#endif

#ifdef AR_EFMT1
	    /* BSD 4.4 extended AR format: #1/<namelen>, with name as the
	     * first <namelen> bytes of the file.  */
	    if (memcmp(memberName, AR_EFMT1, sizeof(AR_EFMT1) - 1) == 0 &&
		isdigit(memberName[sizeof(AR_EFMT1) - 1])) {

		int elen = atoi(memberName + sizeof(AR_EFMT1)-1);

		if (elen <= 0 || elen >= PATH_MAX)
			break;
		memberName = buffer;
		if (fread(memberName, elen, 1, arch) != 1)
			break;
		memberName[elen] = '\0';
		if (fseek(arch, -elen, SEEK_CUR) != 0)
			break;
		if (DEBUG(ARCH) || DEBUG(MAKE))
		    printf("ArchStat: Extended format entry for %s\n", 
		    	memberName);
	    }
#endif

	    ohash_insert(&ar->members,
		ohash_qlookup(&ar->members, memberName),
		    new_arch_member(&arHeader, memberName));
	}
	if (fseek(arch, (size + 1) & ~1, SEEK_CUR) != 0)
	    break;
    }

    fclose(arch);
    ohash_delete(&ar->members);
#ifdef SVR4ARCHIVES
    efree(list.fnametab);
#endif
    free(ar);
    return NULL;
}

/*-
 *-----------------------------------------------------------------------
 * ArchMTimeMember --
 *	Find the modification time of an archive's member, given the
 *	path to the archive and the path to the desired member.
 *
 * Results:
 *	The archive member's modification time, or OUT_OF_DATE if member
 *	was not found (convenient, so that missing members are always
 *	out of date).
 *
 * Side Effects:
 *	Cache the whole archive contents if hash is true.
 *-----------------------------------------------------------------------
 */
static TIMESTAMP
ArchMTimeMember(
    const char	  *archive,   /* Path to the archive */
    const char	  *member,    /* Name of member. If it is a path, only the
			       * last component is used. */
    bool	  hash)       /* true if archive should be hashed if not
			       * already so. */
{
    FILE *	  arch;       /* Stream to archive */
    Arch	  *ar;	      /* Archive descriptor */
    unsigned int  slot;       /* Place of archive in the archives hash */
    const char	  *end = NULL;
    const char	  *cp;
    TIMESTAMP	  result;

    ts_set_out_of_date(result);
    /* Because of space constraints and similar things, files are archived
     * using their final path components, not the entire thing, so we need
     * to point 'member' to the final component, if there is one, to make
     * the comparisons easier...  */
    cp = strrchr(member, '/');
    if (cp != NULL)
	member = cp + 1;

    /* Try to find archive in cache.  */
    slot = ohash_qlookupi(&archives, archive, &end);
    ar = ohash_find(&archives, slot);

    /* If not found, get it now.  */
    if (ar == NULL) {
	if (!hash) {
	    /* Quick path:  no need to hash the whole archive, just use
	     * ArchFindMember to get the member's header and close the stream
	     * again.  */
	    struct ar_hdr	arHeader;

	    arch = ArchFindMember(archive, member, &arHeader, "r");

	    if (arch != NULL) {
		fclose(arch);
		ts_set_from_time_t( (time_t)strtol(arHeader.ar_date, NULL, 10), 
		    result);
	    }
	    return result;
	}
	ar = read_archive(archive, end);
	if (ar != NULL)
	    ohash_insert(&archives, slot, ar);
    }

    /* If archive was found, get entry we seek.  */
    if (ar != NULL) {
	struct arch_member *he;
	end = NULL;

	he = ohash_find(&ar->members, ohash_qlookupi(&ar->members, member, &end));
	if (he != NULL)
	    return mtime_of_member(he);
	else {
	    if ((size_t)(end - member) > AR_NAME_SIZE) {
		/* Try truncated name.	*/
		end = member + AR_NAME_SIZE;
		he = ohash_find(&ar->members,
		    ohash_qlookupi(&ar->members, member, &end));
		if (he != NULL)
		    return mtime_of_member(he);
	    }
	}
    }
    return result;
}

#ifdef SVR4ARCHIVES
/*-
 *-----------------------------------------------------------------------
 * ArchSVR4Entry --
 *	Parse an SVR4 style entry that begins with a slash.
 *	If it is "//", then load the table of filenames
 *	If it is "/<offset>", then try to substitute the long file name
 *	from offset of a table previously read.
 *
 * Results:
 *	svr4list: just read a list of names
 *	NULL:	  error occurred
 *	extended name
 *
 * Side-effect:
 *	For a list of names, store the list in l.
 *-----------------------------------------------------------------------
 */

static char *
ArchSVR4Entry(struct SVR4namelist *l, const char *name, size_t size, FILE *arch)
{
#define ARLONGNAMES1 "/"
#define ARLONGNAMES2 "ARFILENAMES"
    size_t entry;
    char *ptr, *eptr;

    assert(name[0] == '/');
    name++;
    /* First comes a table of archive names, to be used by subsequent calls.  */
    if (memcmp(name, ARLONGNAMES1, sizeof(ARLONGNAMES1) - 1) == 0 ||
	memcmp(name, ARLONGNAMES2, sizeof(ARLONGNAMES2) - 1) == 0) {

	if (l->fnametab != NULL) {
	    if (DEBUG(ARCH))
		printf("Attempted to redefine an SVR4 name table\n");
	    return NULL;
	}

	l->fnametab = emalloc(size);
	l->fnamesize = size;

	if (fread(l->fnametab, size, 1, arch) != 1) {
	    if (DEBUG(ARCH))
		printf("Reading an SVR4 name table failed\n");
	    return NULL;
	}

	eptr = l->fnametab + size;
	for (entry = 0, ptr = l->fnametab; ptr < eptr; ptr++)
	    switch (*ptr) {
	    case '/':
		entry++;
		*ptr = '\0';
		break;

	    case '\n':
		break;

	    default:
		break;
	    }
	if (DEBUG(ARCH))
	    printf("Found svr4 archive name table with %lu entries\n",
			(u_long)entry);
	return (char *)svr4list;
    }
    /* Then the names themselves are given as offsets in this table.  */
    if (*name == ' ' || *name == '\0')
	return NULL;

    entry = (size_t) strtol(name, &eptr, 0);
    if ((*eptr != ' ' && *eptr != '\0') || eptr == name) {
	if (DEBUG(ARCH))
	    printf("Could not parse SVR4 name /%s\n", name);
	return NULL;
    }
    if (entry >= l->fnamesize) {
	if (DEBUG(ARCH))
	    printf("SVR4 entry offset /%s is greater than %lu\n",
		   name, (u_long)l->fnamesize);
	return NULL;
    }

    if (DEBUG(ARCH))
	printf("Replaced /%s with %s\n", name, l->fnametab + entry);

    return l->fnametab + entry;
}
#endif


/*-
 *-----------------------------------------------------------------------
 * ArchFindMember --
 *	Locate a member of an archive, given the path of the archive and
 *	the path of the desired member. If the archive is to be modified,
 *	the mode should be "r+", if not, it should be "r".
 *
 * Results:
 *	A FILE *, opened for reading and writing, positioned right after
 *	the member's header, or NULL if the member was nonexistent.
 *
 * Side Effects:
 *	Fill the struct ar_hdr pointed by arHeaderPtr.
 *-----------------------------------------------------------------------
 */
static FILE *
ArchFindMember(
    const char	  *archive,   /* Path to the archive */
    const char	  *member,    /* Name of member. If it is a path, only the
			       * last component is used. */
    struct ar_hdr *arHeaderPtr,/* Pointer to header structure to be filled in */
    const char	  *mode)      /* mode for opening the stream */
{
    FILE *	  arch;       /* Stream to archive */
    char	  *cp;
    char	  magic[SARMAG];
    size_t	  length;
#ifdef SVR4ARCHIVES
    struct SVR4namelist list;

    list.fnametab = NULL;
#endif

    arch = fopen(archive, mode);
    if (arch == NULL)
	return NULL;

    /* Make sure this is an archive we can handle.  */
    if (fread(magic, SARMAG, 1, arch) != 1 ||
	strncmp(magic, ARMAG, SARMAG) != 0) {
	    fclose(arch);
	    return NULL;
    }

    /* Because of space constraints and similar things, files are archived
     * using their final path components, not the entire thing, so we need
     * to point 'member' to the final component, if there is one, to make
     * the comparisons easier...  */
    cp = strrchr(member, '/');
    if (cp != NULL)
	member = cp + 1;

    length = strlen(member);
    if (length >= AR_NAME_SIZE)
	length = AR_NAME_SIZE;

    /* Error handling is simpler than for read_archive, since we just
     * look for a given member.  */
    while (fread(arHeaderPtr, sizeof(struct ar_hdr), 1, arch) == 1) {
	off_t		  size;       /* Size of archive member */
	char		  *memberName;

	if (memcmp(arHeaderPtr->ar_fmag, ARFMAG, sizeof(arHeaderPtr->ar_fmag) ) 
	    != 0)
	     /* The header is bogus, so the archive is bad.  */
	     break;

	memberName = arHeaderPtr->ar_name;
	if (memcmp(member, memberName, length) == 0) {
	    /* If the member's name doesn't take up the entire 'name' field,
	     * we have to be careful of matching prefixes. Names are space-
	     * padded to the right, so if the character in 'name' at the end
	     * of the matched string is anything but a space, this isn't the
	     * member we sought.  */
#ifdef SVR4ARCHIVES
	    if (length < sizeof(arHeaderPtr->ar_name) && 
	    	memberName[length] == '/')
		length++;
#endif
	    if (length == sizeof(arHeaderPtr->ar_name) ||
		memberName[length] == ' ') {
#ifdef SVR4ARCHIVES
		efree(list.fnametab);
#endif
		return arch;
	    }
	}

	size = (off_t) field2long(arHeaderPtr->ar_size, 
	    sizeof(arHeaderPtr->ar_size));

#ifdef SVR4ARCHIVES
	    /* svr4 names are slash terminated. Also svr4 extended AR format.
	     */
	    if (memberName[0] == '/') {
		/* svr4 magic mode.  */
		memberName = ArchSVR4Entry(&list, arHeaderPtr->ar_name, size, 
		    arch);
		if (memberName == NULL)		/* Invalid data */
		    break;
		else if (memberName == svr4list)/* List of files entry */
		    continue;
		/* Got the entry.  */
		if (strcmp(memberName, member) == 0) {
		    efree(list.fnametab);
		    return arch;
		}
	    }
#endif

#ifdef AR_EFMT1
	/* BSD 4.4 extended AR format: #1/<namelen>, with name as the
	 * first <namelen> bytes of the file.  */
	if (memcmp(memberName, AR_EFMT1, sizeof(AR_EFMT1) - 1) == 0 &&
	    isdigit(memberName[sizeof(AR_EFMT1) - 1])) {
	    char	  ename[PATH_MAX];

	    int elength = atoi(memberName + sizeof(AR_EFMT1)-1);

	    if (elength <= 0 || elength >= PATH_MAX)
		break;
	    if (fread(ename, elength, 1, arch) != 1)
		break;
	    if (fseek(arch, -elength, SEEK_CUR) != 0)
		break;
	    ename[elength] = '\0';
	    if (DEBUG(ARCH) || DEBUG(MAKE))
		printf("ArchFind: Extended format entry for %s\n", ename);
	    /* Found as extended name.	*/
	    if (strcmp(ename, member) == 0) {
#ifdef SVR4ARCHIVES
		efree(list.fnametab);
#endif
		return arch;
		}
	}
#endif
	/* This isn't the member we're after, so we need to advance the
	 * stream's pointer to the start of the next header.  */
	if (fseek(arch, (size + 1) & ~1, SEEK_CUR) != 0)
	    break;
    }

    /* We did not find the member, or we ran into an error while reading
     * the archive.  */
#ifdef SVRARCHIVES
    efree(list.fnametab);
#endif
    fclose(arch);
    return NULL;
}

static void
ArchTouch(const char *archive, const char *member)
{
    FILE *arch;
    struct ar_hdr arHeader;

    arch = ArchFindMember(archive, member, &arHeader, "r+");
    if (arch != NULL) {
	snprintf(arHeader.ar_date, sizeof(arHeader.ar_date), "%-12ld", (long)
	    timestamp2time_t(now));
	if (fseek(arch, -sizeof(struct ar_hdr), SEEK_CUR) == 0)
	    (void)fwrite(&arHeader, sizeof(struct ar_hdr), 1, arch);
	fclose(arch);
    }
}

/*
 * Side Effects:
 *	The modification time of the entire archive is also changed.
 *	For a library, this could necessitate the re-ranlib'ing of the
 *	whole thing.
 */
void
Arch_Touch(GNode *gn)
{
    ArchTouch(Varq_Value(ARCHIVE_INDEX, gn), Varq_Value(MEMBER_INDEX, gn));
}

/*ARGSUSED*/
void
Arch_TouchLib(GNode *gn UNUSED)
                     /* ^          Non RANLIBMAG does nothing with it */
{
#ifdef RANLIBMAG
    if (gn->path != NULL) {
	ArchTouch(gn->path, RANLIBMAG);
	set_times(gn->path);
    }
#endif
}

TIMESTAMP
Arch_MTime(GNode *gn)
{
    gn->mtime = ArchMTimeMember(Varq_Value(ARCHIVE_INDEX, gn),
	     Varq_Value(MEMBER_INDEX, gn),
	     true);

    return gn->mtime;
}

TIMESTAMP
Arch_MemMTime(GNode *gn)
{
    LstNode	  ln;

    for (ln = Lst_First(&gn->parents); ln != NULL; ln = Lst_Adv(ln)) {
	GNode	*pgn;
	char	*nameStart,
		*nameEnd;

	pgn = (GNode *)Lst_Datum(ln);

	if (pgn->type & OP_ARCHV) {
	    /* If the parent is an archive specification and is being made
	     * and its member's name matches the name of the node we were
	     * given, record the modification time of the parent in the
	     * child. We keep searching its parents in case some other
	     * parent requires this child to exist...  */
	    if ((nameStart = strchr(pgn->name, '(') ) != NULL) {
		nameStart++;
		nameEnd = strchr(nameStart, ')');
	    } else
		nameEnd = NULL;

	    if (pgn->make && nameEnd != NULL &&
		strncmp(nameStart, gn->name, nameEnd - nameStart) == 0 &&
		gn->name[nameEnd-nameStart] == '\0')
		    gn->mtime = Arch_MTime(pgn);
	} else if (pgn->make) {
	    /* Something which isn't a library depends on the existence of
	     * this target, so it needs to exist.  */
	    ts_set_out_of_date(gn->mtime);
	    break;
	}
    }
    return gn->mtime;
}

/* If the system can handle the -L flag when linking (or we cannot find 
 * the library), we assume that the user has placed the .LIBRARIES variable 
 * in the final linking command (or the linker will know where to find it) 
 * and set the TARGET variable for this node to be the node's name. Otherwise,
 * we set the TARGET variable to be the full path of the library,
 * as returned by Dir_FindFile.
 */
void
Arch_FindLib(GNode *gn, Lst path)
{
    char	    *libName;	/* file name for archive */
    size_t	    length = strlen(gn->name) + 6 - 2;

    libName = emalloc(length);
    snprintf(libName, length, "lib%s.a", &gn->name[2]);

    gn->path = Dir_FindFile(libName, path);

    free(libName);

#ifdef LIBRARIES
    Varq_Set(TARGET_INDEX, gn->name, gn);
#else
    Varq_Set(TARGET_INDEX, gn->path == NULL ? gn->name : gn->path, gn);
#endif /* LIBRARIES */
}

/*-
 *-----------------------------------------------------------------------
 * Arch_LibOODate --
 *	Decide if a node with the OP_LIB attribute is out-of-date. Called
 *	from Make_OODate to make its life easier.
 *
 *	There are several ways for a library to be out-of-date that are
 *	not available to ordinary files. In addition, there are ways
 *	that are open to regular files that are not available to
 *	libraries. A library that is only used as a source is never
 *	considered out-of-date by itself. This does not preclude the
 *	library's modification time from making its parent be out-of-date.
 *	A library will be considered out-of-date for any of these reasons,
 *	given that it is a target on a dependency line somewhere:
 *	    Its modification time is less than that of one of its
 *		  sources (gn->mtime < gn->cmtime).
 *	    Its modification time is greater than the time at which the
 *		  make began (i.e. it's been modified in the course
 *		  of the make, probably by archiving).
 *	    The modification time of one of its sources is greater than
 *		  the one of its RANLIBMAG member (i.e. its table of contents
 *		  is out-of-date). We don't compare of the archive time
 *		  vs. TOC time because they can be too close. In my
 *		  opinion we should not bother with the TOC at all since
 *		  this is used by 'ar' rules that affect the data contents
 *		  of the archive, not by ranlib rules, which affect the
 *		  TOC.
 *
 * Results:
 *	true if the library is out-of-date. false otherwise.
 *
 * Side Effects:
 *	The library will be hashed if it hasn't been already.
 *-----------------------------------------------------------------------
 */
bool
Arch_LibOODate(GNode *gn)
{
#ifdef RANLIBMAG
    TIMESTAMP	  modTimeTOC;	/* mod time of __.SYMDEF */
#endif

    if (OP_NOP(gn->type) && Lst_IsEmpty(&gn->children))
	return false;
    if (is_strictly_before(now, gn->mtime) || is_strictly_before(gn->mtime, gn->cmtime) ||
	is_out_of_date(gn->mtime))
	return true;
#ifdef RANLIBMAG
    /* non existent libraries are always out-of-date.  */
    if (gn->path == NULL)
	return true;
    modTimeTOC = ArchMTimeMember(gn->path, RANLIBMAG, false);

    if (!is_out_of_date(modTimeTOC)) {
	if (DEBUG(ARCH) || DEBUG(MAKE))
	    printf("%s modified %s...", RANLIBMAG, Targ_FmtTime(modTimeTOC));
	return is_strictly_before(modTimeTOC, gn->cmtime);
    }
    /* A library w/o a table of contents is out-of-date.  */
    if (DEBUG(ARCH) || DEBUG(MAKE))
	printf("No t.o.c....");
    return true;
#else
    return false;
#endif
}

void
Arch_Init(void)
{
    ohash_init(&archives, 4, &arch_info);
}

#ifdef CLEANUP
void
Arch_End(void)
{
    Arch *e;
    unsigned int i;

    for (e = ohash_first(&archives, &i); e != NULL;
	e = ohash_next(&archives, &i))
	    ArchFree(e);
    ohash_delete(&archives);
}
#endif

bool
Arch_IsLib(GNode *gn)
{
    char buf[SARMAG];
    int fd;

    if (gn->path == NULL || (fd = open(gn->path, O_RDONLY)) == -1)
	return false;

    if (read(fd, buf, SARMAG) != SARMAG) {
	(void)close(fd);
	return false;
    }

    (void)close(fd);

    return memcmp(buf, ARMAG, SARMAG) == 0;
}
