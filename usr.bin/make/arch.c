/*	$OpenBSD: arch.c,v 1.37 2000/11/24 14:36:33 espie Exp $	*/
/*	$NetBSD: arch.c,v 1.17 1996/11/06 17:58:59 christos Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

/*-
 * arch.c --
 *	Functions to manipulate libraries, archives and their members.
 *
 *	Once again, cacheing/hashing comes into play in the manipulation
 * of archives. The first time an archive is referenced, all of its members'
 * headers are read and hashed and the archive closed again. All hashed
 * archives are kept on a list which is searched each time an archive member
 * is referenced.
 *
 * The interface to this module is:
 *	Arch_ParseArchive   	Given an archive specification, return a list
 *	    	  	    	of GNode's, one for each member in the spec.
 *	    	  	    	FAILURE is returned if the specification is
 *	    	  	    	invalid for some reason.
 *
 *	Arch_Touch	    	Alter the modification time of the archive
 *	    	  	    	member described by the given node to be
 *	    	  	    	the current time.
 *
 *	Arch_TouchLib	    	Update the modification time of the library
 *	    	  	    	described by the given node. This is special
 *	    	  	    	because it also updates the modification time
 *	    	  	    	of the library's table of contents.
 *
 *	Arch_MTime	    	Find the modification time of a member of
 *	    	  	    	an archive *in the archive*, return TRUE if
 *				exists. The time is placed in the member's 
 *				GNode. Returns the modification time.
 *
 *	Arch_MemMTime	    	Find the modification time of a member of
 *	    	  	    	an archive. Called when the member doesn't
 *	    	  	    	already exist. Looks in the archive for the
 *	    	  	    	modification time. Returns the modification
 *	    	  	    	time.
 *
 *	Arch_FindLib	    	Search for a library along a path. The
 *	    	  	    	library name in the GNode should be in
 *	    	  	    	-l<name> format.
 *
 *	Arch_LibOODate	    	Special function to decide if a library node
 *	    	  	    	is out-of-date.
 *
 *	Arch_Init 	    	Initialize this module.
 *
 *	Arch_End 	    	Cleanup this module.
 */

#include    <sys/types.h>
#include    <sys/stat.h>
#include    <sys/time.h>
#include    <sys/param.h>
#include    <stddef.h>
#include    <ctype.h>
#include    <ar.h>
#include    <utime.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <fcntl.h>
#include    "make.h"
#include    "ohash.h"
#include    "dir.h"
#include    "config.h"

#ifndef lint
#if 0
static char sccsid[] = "@(#)arch.c	8.2 (Berkeley) 1/2/94";
#else
UNUSED
static char rcsid[] = "$OpenBSD: arch.c,v 1.37 2000/11/24 14:36:33 espie Exp $";
#endif
#endif /* not lint */

#ifdef TARGET_MACHINE
#undef MACHINE
#define MACHINE TARGET_MACHINE
#endif
#ifdef TARGET_MACHINE_ARCH
#undef MACHINE_ARCH
#define MACHINE_ARCH TARGET_MACHINE_ARCH
#endif

static LIST	  archives;   /* Lst of archives we've already examined */

typedef struct Arch_ {
    char	  *name;      /* Name of archive */
    struct hash	  members;    /* All the members of this archive, as
     			       * struct arch_member entries.  */
    char	  *fnametab;  /* Extended name table strings */
    size_t	  fnamesize;  /* Size of the string table */
} Arch;

/* Used to get to ar's field sizes.  */
static struct ar_hdr *dummy;
#define AR_NAME_SIZE	    	(sizeof(dummy->ar_name))
#define AR_DATE_SIZE		(sizeof(dummy->ar_date))

/* Each archive member is tied to an arch_member structure, 
 * suitable for hashing.  */
struct arch_member {
    TIMESTAMP	  mtime;	/* Member modification date.  */
    char   	  date[AR_DATE_SIZE+1];
    				/* Same, before conversion to numeric value.  */
    char	  name[1];	/* Member name.  */
};

static struct hash_info members_info = {
    offsetof(struct arch_member, name), NULL, 
    hash_alloc, hash_free, element_alloc 
};

static struct arch_member *new_arch_member __P((struct ar_hdr *, const char *));
static TIMESTAMP mtime_of_member __P((struct arch_member *));

static int ArchFindArchive __P((void *, void *));
#ifdef CLEANUP
static void ArchFree __P((void *));
#endif
static TIMESTAMP ArchMTimeMember __P((char *, char *, Boolean));
static FILE *ArchFindMember __P((char *, char *, struct ar_hdr *, char *));
#if defined(__svr4__) || defined(__SVR4) || \
    (defined(__OpenBSD__) && defined(__mips__)) || \
    (defined(__OpenBSD__) && defined(__powerpc))
#define SVR4ARCHIVES
static int ArchSVR4Entry __P((Arch *, char *, size_t, FILE *));
#endif

static struct arch_member *
new_arch_member(hdr, name)
    struct ar_hdr *hdr;
    const char *name;
{
    const char *end = NULL;
    struct arch_member *n;

    n = hash_create_entry(&members_info, name, &end);
    /* XXX ar entries are NOT null terminated.  */
    memcpy(n->date, &(hdr->ar_date), AR_DATE_SIZE);
    n->date[AR_DATE_SIZE] = '\0';
    /* Don't compute mtime before it is needed. */
    set_out_of_date(n->mtime);
    return n;
}

static TIMESTAMP
mtime_of_member(m)
    struct arch_member *m;
{
    if (is_out_of_date(m->mtime))
    	grab_date((time_t) strtol(m->date, NULL, 10), m->mtime);
    return m->mtime;
}

#ifdef CLEANUP
/*-
 *-----------------------------------------------------------------------
 * ArchFree --
 *	Free memory used by an archive
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static void
ArchFree(ap)
    void *ap;
{
    Arch *a = (Arch *) ap;
    struct arch_member *mem;
    unsigned i;

    /* Free memory from hash entries */
    for (mem = hash_first(&a->members, &i); mem != NULL;
    	mem = hash_next(&a->members, &i))
	free(mem);

    free(a->name);
    efree(a->fnametab);
    hash_delete(&a->members);
    free(a);
}
#endif



/*-
 *-----------------------------------------------------------------------
 * Arch_ParseArchive --
 *	Parse the archive specification in the given line and find/create
 *	the nodes for the specified archive members, placing their nodes
 *	on the given list.
 *
 * Results:
 *	SUCCESS if it was a valid specification. The linePtr is updated
 *	to point to the first non-space after the archive spec. The
 *	nodes for the members are placed on the given list.
 *
 * Side Effects:
 *	Some nodes may be created. The given list is extended.
 *
 *-----------------------------------------------------------------------
 */
ReturnStatus
Arch_ParseArchive(linePtr, nodeLst, ctxt)
    char	    **linePtr;      /* Pointer to start of specification */
    Lst	    	    nodeLst;   	    /* Lst on which to place the nodes */
    SymTable   	    *ctxt;  	    /* Context in which to expand variables */
{
    register char   *cp;	    /* Pointer into line */
    GNode	    *gn;     	    /* New node */
    char	    *libName;  	    /* Library-part of specification */
    char	    *memName;  	    /* Member-part of specification */
    char	    nameBuf[MAKE_BSIZE]; /* temporary place for node name */
    char	    saveChar;  	    /* Ending delimiter of member-name */
    Boolean 	    subLibName;	    /* TRUE if libName should have/had
				     * variable substitution performed on it */

    libName = *linePtr;

    subLibName = FALSE;

    for (cp = libName; *cp != '(' && *cp != '\0'; cp++) {
	if (*cp == '$') {
	    /*
	     * Variable spec, so call the Var module to parse the puppy
	     * so we can safely advance beyond it...
	     */
	    size_t 	length;
	    Boolean	freeIt;
	    char	*result;

	    result=Var_Parse(cp, ctxt, TRUE, &length, &freeIt);
	    if (result == var_Error) {
		return(FAILURE);
	    } else {
		subLibName = TRUE;
	    }

	    if (freeIt) {
		free(result);
	    }
	    cp += length-1;
	}
    }

    *cp++ = '\0';
    if (subLibName) {
	libName = Var_Subst(libName, ctxt, TRUE);
    }


    for (;;) {
	/*
	 * First skip to the start of the member's name, mark that
	 * place and skip to the end of it (either white-space or
	 * a close paren).
	 */
	Boolean	doSubst = FALSE; /* TRUE if need to substitute in memName */

	while (*cp != '\0' && *cp != ')' && isspace (*cp)) {
	    cp++;
	}
	memName = cp;
	while (*cp != '\0' && *cp != ')' && !isspace (*cp)) {
	    if (*cp == '$') {
		/*
		 * Variable spec, so call the Var module to parse the puppy
		 * so we can safely advance beyond it...
		 */
		size_t 	length;
		Boolean	freeIt;
		char	*result;

		result=Var_Parse(cp, ctxt, TRUE, &length, &freeIt);
		if (result == var_Error) {
		    return(FAILURE);
		} else {
		    doSubst = TRUE;
		}

		if (freeIt) {
		    free(result);
		}
		cp += length;
	    } else {
		cp++;
	    }
	}

	/*
	 * If the specification ends without a closing parenthesis,
	 * chances are there's something wrong (like a missing backslash),
	 * so it's better to return failure than allow such things to happen
	 */
	if (*cp == '\0') {
	    printf("No closing parenthesis in archive specification\n");
	    return (FAILURE);
	}

	/*
	 * If we didn't move anywhere, we must be done
	 */
	if (cp == memName) {
	    break;
	}

	saveChar = *cp;
	*cp = '\0';

	/*
	 * XXX: This should be taken care of intelligently by
	 * SuffExpandChildren, both for the archive and the member portions.
	 */
	/*
	 * If member contains variables, try and substitute for them.
	 * This will slow down archive specs with dynamic sources, of course,
	 * since we'll be (non-)substituting them three times, but them's
	 * the breaks -- we need to do this since SuffExpandChildren calls
	 * us, otherwise we could assume the thing would be taken care of
	 * later.
	 */
	if (doSubst) {
	    char    *buf;
	    char    *sacrifice;
	    char    *oldMemName = memName;

	    memName = Var_Subst(memName, ctxt, TRUE);

	    /*
	     * Now form an archive spec and recurse to deal with nested
	     * variables and multi-word variable values.... The results
	     * are just placed at the end of the nodeLst we're returning.
	     */
	    buf = sacrifice = emalloc(strlen(memName)+strlen(libName)+3);

	    sprintf(buf, "%s(%s)", libName, memName);

	    if (strchr(memName, '$') && strcmp(memName, oldMemName) == 0) {
		/*
		 * Must contain dynamic sources, so we can't deal with it now.
		 * Just create an ARCHV node for the thing and let
		 * SuffExpandChildren handle it...
		 */
		gn = Targ_FindNode(buf, TARG_CREATE);

		if (gn == NULL) {
		    free(buf);
		    return(FAILURE);
		} else {
		    gn->type |= OP_ARCHV;
		    Lst_AtEnd(nodeLst, gn);
		}
	    } else if (Arch_ParseArchive(&sacrifice, nodeLst, ctxt)!=SUCCESS) {
		/*
		 * Error in nested call -- free buffer and return FAILURE
		 * ourselves.
		 */
		free(buf);
		return(FAILURE);
	    }
	    /*
	     * Free buffer and continue with our work.
	     */
	    free(buf);
	} else if (Dir_HasWildcards(memName)) {
	    LIST members;
	    char  *member;

	    Lst_Init(&members);
	    Dir_Expand(memName, &dirSearchPath, &members);
	    while ((member = (char *)Lst_DeQueue(&members)) != NULL) {

		sprintf(nameBuf, "%s(%s)", libName, member);
		free(member);
		gn = Targ_FindNode(nameBuf, TARG_CREATE);
		if (gn == NULL)
		    return (FAILURE);
		else {
		    /*
		     * We've found the node, but have to make sure the rest of
		     * the world knows it's an archive member, without having
		     * to constantly check for parentheses, so we type the
		     * thing with the OP_ARCHV bit before we place it on the
		     * end of the provided list.
		     */
		    gn->type |= OP_ARCHV;
		    Lst_AtEnd(nodeLst, gn);
		}
	    }
	    Lst_Destroy(&members, NOFREE);
	} else {
	    sprintf(nameBuf, "%s(%s)", libName, memName);
	    gn = Targ_FindNode (nameBuf, TARG_CREATE);
	    if (gn == NULL) {
		return (FAILURE);
	    } else {
		/*
		 * We've found the node, but have to make sure the rest of the
		 * world knows it's an archive member, without having to
		 * constantly check for parentheses, so we type the thing with
		 * the OP_ARCHV bit before we place it on the end of the
		 * provided list.
		 */
		gn->type |= OP_ARCHV;
		Lst_AtEnd(nodeLst, gn);
	    }
	}
	if (doSubst) {
	    free(memName);
	}

	*cp = saveChar;
    }

    /*
     * If substituted libName, free it now, since we need it no longer.
     */
    if (subLibName) {
	free(libName);
    }

    /*
     * We promised the pointer would be set up at the next non-space, so
     * we must advance cp there before setting *linePtr... (note that on
     * entrance to the loop, cp is guaranteed to point at a ')')
     */
    do {
	cp++;
    } while (*cp != '\0' && isspace (*cp));

    *linePtr = cp;
    return (SUCCESS);
}

/*-
 *-----------------------------------------------------------------------
 * ArchFindArchive --
 *	See if the given archive is the one we are looking for. Called
 *	From ArchMTimeMember and ArchFindMember via Lst_Find.
 *
 * Results:
 *	0 if it is, non-zero if it isn't.
 *-----------------------------------------------------------------------
 */
static int
ArchFindArchive(ar, archName)
    void *ar;	      	  /* Current list element */
    void *archName;  	  /* Name we want */
{
    return strcmp ((char *)archName, ((Arch *)ar)->name);
}

/*-
 *-----------------------------------------------------------------------
 * ArchMTimeMember --
 *	Locate a member of an archive, given the path of the archive and
 *	the path of the desired member.
 *
 * Results:
 *	A pointer to the current struct ar_hdr structure for the member. Note
 *	That no position is returned, so this is not useful for touching
 *	archive members. This is mostly because we have no assurances that
 *	The archive will remain constant after we read all the headers, so
 *	there's not much point in remembering the position...
 *
 * Side Effects:
 *
 *-----------------------------------------------------------------------
 */
static TIMESTAMP
ArchMTimeMember(archive, member, hash)
    char	  *archive;   /* Path to the archive */
    char	  *member;    /* Name of member. If it is a path, only the
			       * last component is used. */
    Boolean	  hash;	      /* TRUE if archive should be hashed if not
    			       * already so. */
{
#define AR_MAX_NAME_LEN	    (sizeof(arh.ar_name)-1)
    FILE *	  arch;	      /* Stream to archive */
    int		  size;       /* Size of archive member */
    char	  *cp;	      /* Useful character pointer */
    char	  magic[SARMAG];
    LstNode	  ln;	      /* Lst member containing archive descriptor */
    Arch	  *ar;	      /* Archive descriptor */
    struct arch_member *he;   /* Entry containing member's description */
    struct ar_hdr arh;        /* archive-member header for reading archive */
    char	  memName[MAXPATHLEN+1];
    	    	    	    /* Current member name while hashing. */
    const char 	  *end = NULL;
    TIMESTAMP	  result;

    set_out_of_date(result);

    /*
     * Because of space constraints and similar things, files are archived
     * using their final path components, not the entire thing, so we need
     * to point 'member' to the final component, if there is one, to make
     * the comparisons easier...
     */
    cp = strrchr(member, '/');
    if (cp != NULL)
	member = cp + 1;

    ln = Lst_Find(&archives, ArchFindArchive, archive);
    if (ln != NULL) {
	ar = (Arch *)Lst_Datum(ln);
	end = NULL;
	he = hash_find(&ar->members, hash_qlookupi(&ar->members, member, &end));
	if (he != NULL)
	    return mtime_of_member(he);
	else {
	    if (end - member > AR_NAME_SIZE) {
		/* Try truncated name */
	    	end = member + AR_NAME_SIZE;

		he = hash_find(&ar->members,
		    hash_qlookupi(&ar->members, member, &end));
		if (he != NULL)
		    return mtime_of_member(he);
	    }
	    return result;
	}
    }

    if (!hash) {
	/*
	 * Caller doesn't want the thing hashed, just use ArchFindMember
	 * to read the header for the member out and close down the stream
	 * again. Since the archive is not to be hashed, we assume there's
	 * no need to allocate extra room for the header we're returning,
	 * so just declare it static.
	 */
	 static struct ar_hdr	sarh;

	 arch = ArchFindMember(archive, member, &sarh, "r");

	if (arch == NULL)
	    return result;
	else {
	    fclose(arch);
	    grab_date( (time_t)strtol(sarh.ar_date, NULL, 10), result);
	    return result;
	}
    }

    /*
     * We don't have this archive on the list yet, so we want to find out
     * everything that's in it and cache it so we can get at it quickly.
     */
    arch = fopen(archive, "r");
    if (arch == NULL)
	return result;

    /*
     * We use the ARMAG string to make sure this is an archive we
     * can handle...
     */
    if ((fread(magic, SARMAG, 1, arch) != 1) ||
    	(strncmp(magic, ARMAG, SARMAG) != 0)) {
	    fclose(arch);
	    return result;
    }

    ar = (Arch *)emalloc(sizeof (Arch));
    ar->name = estrdup(archive);
    ar->fnametab = NULL;
    ar->fnamesize = 0;
    hash_init(&ar->members, 8, &members_info);
    memName[AR_MAX_NAME_LEN] = '\0';

    while (fread((char *)&arh, sizeof (struct ar_hdr), 1, arch) == 1) {
	if (strncmp( arh.ar_fmag, ARFMAG, sizeof (arh.ar_fmag)) != 0) {
	    /*
	     * The header is bogus, so the archive is bad
	     * and there's no way we can recover...
	     */
	    goto badarch;
	} else {
	    /*
	     * We need to advance the stream's pointer to the start of the
	     * next header. Files are padded with newlines to an even-byte
	     * boundary, so we need to extract the size of the file from the
	     * 'size' field of the header and round it up during the seek.
	     */
	    arh.ar_size[sizeof(arh.ar_size)-1] = '\0';
	    size = (int) strtol(arh.ar_size, NULL, 10);

	    (void) strncpy(memName, arh.ar_name, sizeof(arh.ar_name));
	    for (cp = &memName[AR_MAX_NAME_LEN]; *cp == ' '; cp--) {
		continue;
	    }
	    cp[1] = '\0';

#ifdef SVR4ARCHIVES
	    /*
	     * svr4 names are slash terminated. Also svr4 extended AR format.
	     */
	    if (memName[0] == '/') {
		/*
		 * svr4 magic mode; handle it
		 */
		switch (ArchSVR4Entry(ar, memName, size, arch)) {
		case -1:  /* Invalid data */
		    goto badarch;
		case 0:	  /* List of files entry */
		    continue;
		default:  /* Got the entry */
		    break;
		}
	    }
	    else {
		if (cp[0] == '/')
		    cp[0] = '\0';
	    }
#endif

#ifdef AR_EFMT1
	    /*
	     * BSD 4.4 extended AR format: #1/<namelen>, with name as the
	     * first <namelen> bytes of the file
	     */
	    if (strncmp(memName, AR_EFMT1, sizeof(AR_EFMT1) - 1) == 0 &&
		isdigit(memName[sizeof(AR_EFMT1) - 1])) {

		unsigned int elen = atoi(&memName[sizeof(AR_EFMT1)-1]);

		if (elen > MAXPATHLEN)
			goto badarch;
		if (fread (memName, elen, 1, arch) != 1)
			goto badarch;
		memName[elen] = '\0';
		fseek(arch, -elen, SEEK_CUR);
		if (DEBUG(ARCH) || DEBUG(MAKE)) {
		    printf("ArchStat: Extended format entry for %s\n", memName);
		}
	    }
#endif

	    hash_insert(&ar->members,
		hash_qlookup(&ar->members, memName),
		    new_arch_member(&arh, memName));
	}
	fseek(arch, (size + 1) & ~1, SEEK_CUR);
    }

    fclose(arch);

    Lst_AtEnd(&archives, ar);

    /*
     * Now that the archive has been read and cached, we can look into
     * the hash table to find the desired member's header.
     */
    he = hash_find(&ar->members,
	hash_qlookupi(&ar->members, member, &end));

    if (he != NULL)
	return mtime_of_member(he);
    else
	return result;

badarch:
    fclose(arch);
    hash_delete(&ar->members);
    efree(ar->fnametab);
    free(ar);
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
 *	-1: Bad data in archive
 *	 0: A table was loaded from the file
 *	 1: Name was successfully substituted from table
 *	 2: Name was not successfully substituted from table
 *
 * Side Effects:
 *	If a table is read, the file pointer is moved to the next archive
 *	member
 *
 *-----------------------------------------------------------------------
 */
static int
ArchSVR4Entry(ar, name, size, arch)
	Arch *ar;
	char *name;
	size_t size;
	FILE *arch;
{
#define ARLONGNAMES1 "//"
#define ARLONGNAMES2 "/ARFILENAMES"
    size_t entry;
    char *ptr, *eptr;

    if (strncmp(name, ARLONGNAMES1, sizeof(ARLONGNAMES1) - 1) == 0 ||
	strncmp(name, ARLONGNAMES2, sizeof(ARLONGNAMES2) - 1) == 0) {

	if (ar->fnametab != NULL) {
	    if (DEBUG(ARCH)) {
		printf("Attempted to redefine an SVR4 name table\n");
	    }
	    return -1;
	}

	/*
	 * This is a table of archive names, so we build one for
	 * ourselves
	 */
	ar->fnametab = emalloc(size);
	ar->fnamesize = size;

	if (fread(ar->fnametab, size, 1, arch) != 1) {
	    if (DEBUG(ARCH)) {
		printf("Reading an SVR4 name table failed\n");
	    }
	    return -1;
	}
	eptr = ar->fnametab + size;
	for (entry = 0, ptr = ar->fnametab; ptr < eptr; ptr++)
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
	if (DEBUG(ARCH)) {
	    printf("Found svr4 archive name table with %lu entries\n", 
		 	(u_long)entry);
	}
	return 0;
    }

    if (name[1] == ' ' || name[1] == '\0')
	return 2;

    entry = (size_t) strtol(&name[1], &eptr, 0);
    if ((*eptr != ' ' && *eptr != '\0') || eptr == &name[1]) {
	if (DEBUG(ARCH)) {
	    printf("Could not parse SVR4 name %s\n", name);
	}
	return 2;
    }
    if (entry >= ar->fnamesize) {
	if (DEBUG(ARCH)) {
	    printf("SVR4 entry offset %s is greater than %lu\n",
		   name, (u_long)ar->fnamesize);
	}
	return 2;
    }

    if (DEBUG(ARCH)) {
	printf("Replaced %s with %s\n", name, &ar->fnametab[entry]);
    }

    (void) strncpy(name, &ar->fnametab[entry], MAXPATHLEN);
    name[MAXPATHLEN] = '\0';
    return 1;
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
 *	An FILE *, opened for reading and writing, positioned at the
 *	start of the member's struct ar_hdr, or NULL if the member was
 *	nonexistent. The current struct ar_hdr for member.
 *
 * Side Effects:
 *	The passed struct ar_hdr structure is filled in.
 *
 *-----------------------------------------------------------------------
 */
static FILE *
ArchFindMember (archive, member, arhPtr, mode)
    char	  *archive;   /* Path to the archive */
    char	  *member;    /* Name of member. If it is a path, only the
			       * last component is used. */
    struct ar_hdr *arhPtr;    /* Pointer to header structure to be filled in */
    char	  *mode;      /* The mode for opening the stream */
{
    FILE *	  arch;	      /* Stream to archive */
    int		  size;       /* Size of archive member */
    char	  *cp;	      /* Useful character pointer */
    char	  magic[SARMAG];
    int		  len, tlen;

    arch = fopen (archive, mode);
    if (arch == NULL) {
	return (NULL);
    }

    /*
     * We use the ARMAG string to make sure this is an archive we
     * can handle...
     */
    if ((fread (magic, SARMAG, 1, arch) != 1) ||
    	(strncmp (magic, ARMAG, SARMAG) != 0)) {
	    fclose (arch);
	    return (NULL);
    }

    /*
     * Because of space constraints and similar things, files are archived
     * using their final path components, not the entire thing, so we need
     * to point 'member' to the final component, if there is one, to make
     * the comparisons easier...
     */
    cp = strrchr (member, '/');
    if (cp != (char *) NULL) {
	member = cp + 1;
    }
    len = tlen = strlen (member);
    if (len > sizeof (arhPtr->ar_name)) {
	tlen = sizeof (arhPtr->ar_name);
    }

    while (fread ((char *)arhPtr, sizeof (struct ar_hdr), 1, arch) == 1) {
	if (strncmp(arhPtr->ar_fmag, ARFMAG, sizeof (arhPtr->ar_fmag) ) != 0) {
	     /*
	      * The header is bogus, so the archive is bad
	      * and there's no way we can recover...
	      */
	     fclose (arch);
	     return (NULL);
	} else if (strncmp (member, arhPtr->ar_name, tlen) == 0) {
	    /*
	     * If the member's name doesn't take up the entire 'name' field,
	     * we have to be careful of matching prefixes. Names are space-
	     * padded to the right, so if the character in 'name' at the end
	     * of the matched string is anything but a space, this isn't the
	     * member we sought.
	     */
	    if (tlen != sizeof(arhPtr->ar_name) && arhPtr->ar_name[tlen] != ' '){
		goto skip;
	    } else {
		/*
		 * To make life easier, we reposition the file at the start
		 * of the header we just read before we return the stream.
		 * In a more general situation, it might be better to leave
		 * the file at the actual member, rather than its header, but
		 * not here...
		 */
		fseek (arch, -sizeof(struct ar_hdr), SEEK_CUR);
		return (arch);
	    }
	} else
#ifdef AR_EFMT1
		/*
		 * BSD 4.4 extended AR format: #1/<namelen>, with name as the
		 * first <namelen> bytes of the file
		 */
	    if (strncmp(arhPtr->ar_name, AR_EFMT1,
					sizeof(AR_EFMT1) - 1) == 0 &&
		isdigit(arhPtr->ar_name[sizeof(AR_EFMT1) - 1])) {

		unsigned int elen = atoi(&arhPtr->ar_name[sizeof(AR_EFMT1)-1]);
		char ename[MAXPATHLEN];

		if (elen > MAXPATHLEN) {
			fclose (arch);
			return NULL;
		}
		if (fread (ename, elen, 1, arch) != 1) {
			fclose (arch);
			return NULL;
		}
		ename[elen] = '\0';
		if (DEBUG(ARCH) || DEBUG(MAKE)) {
		    printf("ArchFind: Extended format entry for %s\n", ename);
		}
		if (strncmp(ename, member, len) == 0) {
			/* Found as extended name */
			fseek (arch, -sizeof(struct ar_hdr) - elen, SEEK_CUR);
			return (arch);
		}
		fseek (arch, -elen, SEEK_CUR);
		goto skip;
	} else
#endif
	{
skip:
	    /*
	     * This isn't the member we're after, so we need to advance the
	     * stream's pointer to the start of the next header. Files are
	     * padded with newlines to an even-byte boundary, so we need to
	     * extract the size of the file from the 'size' field of the
	     * header and round it up during the seek.
	     */
	    arhPtr->ar_size[sizeof(arhPtr->ar_size)-1] = '\0';
	    size = (int) strtol(arhPtr->ar_size, NULL, 10);
	    fseek (arch, (size + 1) & ~1, SEEK_CUR);
	}
    }

    /*
     * We've looked everywhere, but the member is not to be found. Close the
     * archive and return NULL -- an error.
     */
    fclose (arch);
    return (NULL);
}

/*-
 *-----------------------------------------------------------------------
 * Arch_Touch --
 *	Touch a member of an archive.
 *
 * Results:
 *	The 'time' field of the member's header is updated.
 *
 * Side Effects:
 *	The modification time of the entire archive is also changed.
 *	For a library, this could necessitate the re-ranlib'ing of the
 *	whole thing.
 *
 *-----------------------------------------------------------------------
 */
void
Arch_Touch (gn)
    GNode	  *gn;	  /* Node of member to touch */
{
    FILE *	  arch;	  /* Stream open to archive, positioned properly */
    struct ar_hdr arh;	  /* Current header describing member */

    arch = ArchFindMember(Varq_Value(ARCHIVE_INDEX, gn),
			  Varq_Value(MEMBER_INDEX, gn),
			  &arh, "r+");
    sprintf(arh.ar_date, "%-12ld", (long) timestamp2time_t(now));

    if (arch != NULL) {
	(void)fwrite ((char *)&arh, sizeof (struct ar_hdr), 1, arch);
	fclose (arch);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Arch_TouchLib --
 *	Given a node which represents a library, touch the thing, making
 *	sure that the table of contents also is touched.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Both the modification time of the library and of the RANLIBMAG
 *	member are set to 'now'.
 *
 *-----------------------------------------------------------------------
 */
void
Arch_TouchLib (gn)
    GNode	    *gn;      	/* The node of the library to touch */
{
#ifdef RANLIBMAG
    FILE *	    arch;	/* Stream open to archive */
    struct ar_hdr   arh;      	/* Header describing table of contents */

    arch = ArchFindMember (gn->path, RANLIBMAG, &arh, "r+");
    sprintf(arh.ar_date, "%-12ld", (long) timestamp2time_t(now));

    if (arch != NULL) {
	(void)fwrite ((char *)&arh, sizeof (struct ar_hdr), 1, arch);
	fclose (arch);

	set_times(gn->path);
    }
#endif
}

/*-
 *-----------------------------------------------------------------------
 * Arch_MTime --
 *	Return the modification time of a member of an archive.
 *
 * Results:
 *	The modification time (seconds).
 *
 * Side Effects:
 *	The mtime field of the given node is filled in with the value
 *	returned by the function.
 *-----------------------------------------------------------------------
 */
TIMESTAMP
Arch_MTime(gn)
    GNode	  *gn;	      /* Node describing archive member */
{
    gn->mtime = ArchMTimeMember(Varq_Value(ARCHIVE_INDEX, gn),
	Varq_Value(MEMBER_INDEX, gn),
	TRUE);

    return gn->mtime;
}

/*-
 *-----------------------------------------------------------------------
 * Arch_MemMTime --
 *	Given a non-existent archive member's node, get its modification
 *	time from its archived form, if it exists.
 *
 *-----------------------------------------------------------------------
 */
TIMESTAMP
Arch_MemMTime (gn)
    GNode   	  *gn;
{
    LstNode 	  ln;

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
 	    set_out_of_date(gn->mtime);
	    break;
	}
    }
    return gn->mtime;
}

/*-
 *-----------------------------------------------------------------------
 * Arch_FindLib --
 *	Search for a library along the given search path.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The node's 'path' field is set to the found path (including the
 *	actual file name, not -l...). If the system can handle the -L
 *	flag when linking (or we cannot find the library), we assume that
 *	the user has placed the .LIBRARIES variable in the final linking
 *	command (or the linker will know where to find it) and set the
 *	TARGET variable for this node to be the node's name. Otherwise,
 *	we set the TARGET variable to be the full path of the library,
 *	as returned by Dir_FindFile.
 *
 *-----------------------------------------------------------------------
 */
void
Arch_FindLib (gn, path)
    GNode	    *gn;	      /* Node of library to find */
    Lst	    	    path;	      /* Search path */
{
    char	    *libName;   /* file name for archive */

    libName = (char *)emalloc (strlen (gn->name) + 6 - 2);
    sprintf(libName, "lib%s.a", &gn->name[2]);

    gn->path = Dir_FindFile (libName, path);

    free (libName);

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
 *	    	  sources (gn->mtime < gn->cmtime).
 *	    Its modification time is greater than the time at which the
 *	    	  make began (i.e. it's been modified in the course
 *	    	  of the make, probably by archiving).
 *	    The modification time of one of its sources is greater than
 *		  the one of its RANLIBMAG member (i.e. its table of contents
 *	    	  is out-of-date). We don't compare of the archive time
 *		  vs. TOC time because they can be too close. In my
 *		  opinion we should not bother with the TOC at all since
 *		  this is used by 'ar' rules that affect the data contents
 *		  of the archive, not by ranlib rules, which affect the
 *		  TOC.
 *
 * Results:
 *	TRUE if the library is out-of-date. FALSE otherwise.
 *
 * Side Effects:
 *	The library will be hashed if it hasn't been already.
 *
 *-----------------------------------------------------------------------
 */
Boolean
Arch_LibOODate (gn)
    GNode   	  *gn;  	/* The library's graph node */
{
    TIMESTAMP	modTimeTOC;	/* mod time of __.SYMDEF */

    if (OP_NOP(gn->type) && Lst_IsEmpty(&gn->children)) 
	return FALSE;
    if (is_before(now, gn->mtime) || is_before(gn->mtime, gn->cmtime) ||
    	is_out_of_date(gn->mtime))
	return TRUE;
#ifdef RANLIBMAG
    /* non existent libraries are always out-of-date.  */
    if (gn->path == NULL)
    	return TRUE;

    modTimeTOC = ArchMTimeMember(gn->path, RANLIBMAG, FALSE);

    if (!is_out_of_date(modTimeTOC)) {
	if (DEBUG(ARCH) || DEBUG(MAKE))
	    printf("%s modified %s...", RANLIBMAG, Targ_FmtTime(modTimeTOC));
	return is_before(modTimeTOC, gn->cmtime);
    }
    /* A library w/o a table of contents is out-of-date */
    if (DEBUG(ARCH) || DEBUG(MAKE))
	printf("No t.o.c....");
    return TRUE;
#else
    return FALSE;
#endif
}

/*-
 *-----------------------------------------------------------------------
 * Arch_Init --
 *	Initialize things for this module.
 *
 * Side Effects:
 *	The 'archives' list is initialized.
 *
 *-----------------------------------------------------------------------
 */
void
Arch_Init()
{
    Lst_Init(&archives);
}



/*-
 *-----------------------------------------------------------------------
 * Arch_End --
 *	Cleanup things for this module.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The 'archives' list is freed
 *
 *-----------------------------------------------------------------------
 */
void
Arch_End ()
{
#ifdef CLEANUP
    Lst_Destroy(&archives, ArchFree);
#endif
}

/*-
 *-----------------------------------------------------------------------
 * Arch_IsLib --
 *	Check if the node is a library
 *
 * Results:
 *	True or False.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
int
Arch_IsLib(gn)
    GNode *gn;
{
    static const char armag[] = "!<arch>\n";
    char buf[sizeof(armag)-1];
    int fd;

    if ((fd = open(gn->path, O_RDONLY)) == -1)
	return FALSE;

    if (read(fd, buf, sizeof(buf)) != sizeof(buf)) {
	(void) close(fd);
	return FALSE;
    }

    (void) close(fd);

    return memcmp(buf, armag, sizeof(buf)) == 0;
}
