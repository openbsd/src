/* $OpenBSD: uidswap.c,v 1.41 2018/07/18 11:34:04 dtucker Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Code for uid-swapping.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include <errno.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>

#include "log.h"
#include "uidswap.h"

/*
 * Note: all these functions must work in all of the following cases:
 *    1. euid=0, ruid=0
 *    2. euid=0, ruid!=0
 *    3. euid!=0, ruid!=0
 * Additionally, they must work regardless of whether the system has
 * POSIX saved uids or not.
 */

/* Lets assume that posix saved ids also work with seteuid, even though that
   is not part of the posix specification. */

/* Saved effective uid. */
static int	privileged = 0;
static int	temporarily_use_uid_effective = 0;
static uid_t	saved_euid, user_groups_uid;
static gid_t	saved_egid;
static gid_t	saved_egroups[NGROUPS_MAX], user_groups[NGROUPS_MAX];
static int	saved_egroupslen = -1, user_groupslen = -1;

/*
 * Temporarily changes to the given uid.  If the effective user
 * id is not root, this does nothing.  This call cannot be nested.
 */
void
temporarily_use_uid(struct passwd *pw)
{
	/* Save the current euid, and egroups. */
	saved_euid = geteuid();
	saved_egid = getegid();
	debug("temporarily_use_uid: %u/%u (e=%u/%u)",
	    (u_int)pw->pw_uid, (u_int)pw->pw_gid,
	    (u_int)saved_euid, (u_int)saved_egid);
	if (saved_euid != 0) {
		privileged = 0;
		return;
	}
	privileged = 1;
	temporarily_use_uid_effective = 1;
	saved_egroupslen = getgroups(NGROUPS_MAX, saved_egroups);
	if (saved_egroupslen < 0)
		fatal("getgroups: %.100s", strerror(errno));

	/* set and save the user's groups */
	if (user_groupslen == -1 || user_groups_uid != pw->pw_uid) {
		if (initgroups(pw->pw_name, pw->pw_gid) < 0)
			fatal("initgroups: %s: %.100s", pw->pw_name,
			    strerror(errno));
		user_groupslen = getgroups(NGROUPS_MAX, user_groups);
		if (user_groupslen < 0)
			fatal("getgroups: %.100s", strerror(errno));
		user_groups_uid = pw->pw_uid;
	}
	/* Set the effective uid to the given (unprivileged) uid. */
	if (setgroups(user_groupslen, user_groups) < 0)
		fatal("setgroups: %.100s", strerror(errno));
	if (setegid(pw->pw_gid) < 0)
		fatal("setegid %u: %.100s", (u_int)pw->pw_gid,
		    strerror(errno));
	if (seteuid(pw->pw_uid) == -1)
		fatal("seteuid %u: %.100s", (u_int)pw->pw_uid,
		    strerror(errno));
}

/*
 * Restores to the original (privileged) uid.
 */
void
restore_uid(void)
{
	/* it's a no-op unless privileged */
	if (!privileged) {
		debug("restore_uid: (unprivileged)");
		return;
	}
	if (!temporarily_use_uid_effective)
		fatal("restore_uid: temporarily_use_uid not effective");
	debug("restore_uid: %u/%u", (u_int)saved_euid, (u_int)saved_egid);
	/* Set the effective uid back to the saved privileged uid. */
	if (seteuid(saved_euid) < 0)
		fatal("seteuid %u: %.100s", (u_int)saved_euid, strerror(errno));
	if (setgroups(saved_egroupslen, saved_egroups) < 0)
		fatal("setgroups: %.100s", strerror(errno));
	if (setegid(saved_egid) < 0)
		fatal("setegid %u: %.100s", (u_int)saved_egid, strerror(errno));
	temporarily_use_uid_effective = 0;
}

/*
 * Permanently sets all uids to the given uid.  This cannot be
 * called while temporarily_use_uid is effective.
 */
void
permanently_set_uid(struct passwd *pw)
{
	if (temporarily_use_uid_effective)
		fatal("permanently_set_uid: temporarily_use_uid effective");
	debug("permanently_set_uid: %u/%u", (u_int)pw->pw_uid,
	    (u_int)pw->pw_gid);
	if (setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) != 0)
		fatal("setresgid %u: %s", (u_int)pw->pw_gid, strerror(errno));
	if (setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) != 0)
		fatal("setresuid %u: %s", (u_int)pw->pw_uid, strerror(errno));
}
