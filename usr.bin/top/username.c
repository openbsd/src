/*	$OpenBSD: username.c,v 1.7 2002/07/15 17:20:36 deraadt Exp $	*/

/*
 *  Top users/processes display for Unix
 *  Version 3
 *
 * Copyright (c) 1984, 1989, William LeFebvre, Rice University
 * Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS EMPLOYER BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  Username translation code for top.
 *
 *  These routines handle uid to username mapping.
 *  They use a hashing table scheme to reduce reading overhead.
 *  For the time being, these are very straightforward hashing routines.
 *  Maybe someday I'll put in something better.  But with the advent of
 *  "random access" password files, it might not be worth the effort.
 *
 *  Changes to these have been provided by John Gilmore (gnu@toad.com).
 *
 *  The hash has been simplified in this release, to avoid the
 *  table overflow problems of previous releases.  If the value
 *  at the initial hash location is not right, it is replaced
 *  by the right value.  Collisions will cause us to call getpw*
 *  but hey, this is a cache, not the Library of Congress.
 *  This makes the table size independent of the passwd file size.
 */

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>

#include "top.local.h"
#include "utils.h"

struct hash_el {
    uid_t  uid;
    char name[9];
};

static int enter_user(uid_t, char *, int);
static int get_user(uid_t);

#define    is_empty_hash(x)	(hash_table[x].name[0] == 0)

/* simple minded hashing function */
/* Uid "nobody" is -2 results in hashit(-2) = -2 which is out of bounds for
   the hash_table.  Applied abs() function to fix. 2/16/96 tpugh
*/
#define    hashit(i)	(abs(i) % Table_size)

/* K&R requires that statically declared tables be initialized to zero. */
/* We depend on that for hash_table and YOUR compiler had BETTER do it! */
struct hash_el hash_table[Table_size];

void init_hash()

{
    /*
     *  There used to be some steps we had to take to initialize things.
     *  We don't need to do that anymore, but we will leave this stub in
     *  just in case future changes require initialization steps.
     */
}

char *username(uid)

uid_t uid;

{
    int hashindex;

    hashindex = hashit(uid);
    if (is_empty_hash(hashindex) || (hash_table[hashindex].uid != uid))
    {
	/* not here or not right -- get it out of passwd */
	hashindex = get_user(uid);
    }
    return(hash_table[hashindex].name);
}

uid_t userid(username)

char *username;

{
    struct passwd *pwd;

    /* Eventually we want this to enter everything in the hash table,
       but for now we just do it simply and remember just the result.
     */

    if ((pwd = getpwnam(username)) == NULL)
    {
	return(-1);
    }

    /* enter the result in the hash table */
    enter_user(pwd->pw_uid, username, 1);

    /* return our result */
    return(pwd->pw_uid);
}

static int enter_user(uid, name, wecare)

uid_t  uid;
char *name;
int wecare;		/* 1 = enter it always, 0 = nice to have */

{
    int hashindex;

#ifdef DEBUG
    fprintf(stderr, "enter_hash(%u, %s, %d)\n", uid, name, wecare);
#endif

    hashindex = hashit(uid);

    if (!is_empty_hash(hashindex))
    {
	if (!wecare)
	    return 0;		/* Don't clobber a slot for trash */
	if (hash_table[hashindex].uid == uid)
	    return(hashindex);	/* Fortuitous find */
    }

    /* empty or wrong slot -- fill it with new value */
    hash_table[hashindex].uid = uid;
    (void) strlcpy(hash_table[hashindex].name, name,
               sizeof(hash_table[hashindex].name));
    return(hashindex);
}

/*
 * Get a userid->name mapping from the system.
 * If the passwd database is hashed (#define RANDOM_PW), we
 * just handle this uid.  Otherwise we scan the passwd file
 * and cache any entries we pass over while looking.
 */

static int get_user(uid)

uid_t uid;

{
    struct passwd *pwd;

#ifdef RANDOM_PW
    /* no performance penalty for using getpwuid makes it easy */
    if ((pwd = getpwuid(uid)) != NULL)
    {
	return(enter_user(pwd->pw_uid, pwd->pw_name, 1));
    }
#else

    int from_start = 0;

    /*
     *  If we just called getpwuid each time, things would be very slow
     *  since that just iterates through the passwd file each time.  So,
     *  we walk through the file instead (using getpwent) and cache each
     *  entry as we go.  Once the right record is found, we cache it and
     *  return immediately.  The next time we come in, getpwent will get
     *  the next record.  In theory, we never have to read the passwd file
     *  a second time (because we cache everything we read).  But in
     *  practice, the cache may not be large enough, so if we don't find
     *  it the first time we have to scan the file a second time.  This
     *  is not very efficient, but it will do for now.
     */

    while (from_start++ < 2)
    {
	while ((pwd = getpwent()) != NULL)
	{
	    if (pwd->pw_uid == uid)
	    {
		return(enter_user(pwd->pw_uid, pwd->pw_name, 1));
	    }
	    (void) enter_user(pwd->pw_uid, pwd->pw_name, 0);
	}
	/* try again */
	setpwent();
    }
#endif
    /* if we can't find the name at all, then use the uid as the name */
    return(enter_user(uid, itoa7(uid), 1));
}
