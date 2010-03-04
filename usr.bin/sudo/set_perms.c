/*
 * Copyright (c) 1994-1996,1998-2009 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include <config.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <pwd.h>
#include <errno.h>
#include <grp.h>
#ifdef HAVE_LOGIN_CAP_H
# include <login_cap.h>
#endif

#include "sudo.h"

#ifdef __TANDEM
# define ROOT_UID	65535
#else
# define ROOT_UID	0
#endif

/*
 * Prototypes
 */
static void runas_setup		__P((void));
static void runas_setgroups	__P((void));
static void restore_groups	__P((void));

static int current_perm = -1;

#ifdef HAVE_SETRESUID
/*
 * Set real and effective and saved uids and gids based on perm.
 * We always retain a saved uid of 0 unless we are headed for an exec().
 * We only flip the effective gid since it only changes for PERM_SUDOERS.
 * This version of set_perms() works fine with the "stay_setuid" option.
 */
int
set_perms(perm)
    int perm;
{
    const char *errstr;
    int noexit;

    noexit = ISSET(perm, PERM_NOEXIT);
    CLR(perm, PERM_MASK);

    if (perm == current_perm)
	return(1);

    switch (perm) {
	case PERM_ROOT:
				if (setresuid(ROOT_UID, ROOT_UID, ROOT_UID)) {
				    errstr = "setresuid(ROOT_UID, ROOT_UID, ROOT_UID)";
				    goto bad;
				}
				(void) setresgid(-1, user_gid, -1);
				if (current_perm == PERM_RUNAS)
				    restore_groups();
			      	break;

	case PERM_USER:
    	    	    	        (void) setresgid(-1, user_gid, -1);
				if (setresuid(user_uid, user_uid, ROOT_UID)) {
				    errstr = "setresuid(user_uid, user_uid, ROOT_UID)";
				    goto bad;
				}
			      	break;
				
	case PERM_FULL_USER:
				/* headed for exec() */
    	    	    	        (void) setgid(user_gid);
				if (setresuid(user_uid, user_uid, user_uid)) {
				    errstr = "setresuid(user_uid, user_uid, user_uid)";
				    goto bad;
				}
			      	break;
				
	case PERM_RUNAS:
				runas_setgroups();
				(void) setresgid(-1, runas_gr ?
				    runas_gr->gr_gid : runas_pw->pw_gid, -1);
				if (setresuid(-1, runas_pw ? runas_pw->pw_uid :
				    user_uid, -1)) {
				    errstr = "unable to change to runas uid";
				    goto bad;
				}
			      	break;

	case PERM_FULL_RUNAS:
				/* headed for exec(), assume euid == ROOT_UID */
				runas_setup();
				if (setresuid(def_stay_setuid ?
				    user_uid : runas_pw->pw_uid,
				    runas_pw->pw_uid, runas_pw->pw_uid)) {
				    errstr = "unable to change to runas uid";
				    goto bad;
				}
				break;

	case PERM_SUDOERS:
				/* assume euid == ROOT_UID, ruid == user */
				if (setresgid(-1, SUDOERS_GID, -1))
				    error(1, "unable to change to sudoers gid");

				/*
				 * If SUDOERS_UID == ROOT_UID and SUDOERS_MODE
				 * is group readable we use a non-zero
				 * uid in order to avoid NFS lossage.
				 * Using uid 1 is a bit bogus but should
				 * work on all OS's.
				 */
				if (SUDOERS_UID == ROOT_UID) {
				    if ((SUDOERS_MODE & 040) && setresuid(ROOT_UID, 1, ROOT_UID)) {
					errstr = "setresuid(ROOT_UID, 1, ROOT_UID)";
					goto bad;
				    }
				} else {
				    if (setresuid(ROOT_UID, SUDOERS_UID, ROOT_UID)) {
					errstr = "setresuid(ROOT_UID, SUDOERS_UID, ROOT_UID)";
					goto bad;
				    }
				}
			      	break;
	case PERM_TIMESTAMP:
				if (setresuid(ROOT_UID, timestamp_uid, ROOT_UID)) {
				    errstr = "setresuid(ROOT_UID, timestamp_uid, ROOT_UID)";
				    goto bad;
				}
			      	break;
    }

    current_perm = perm;
    return(1);
bad:
    warningx("%s: %s", errstr,
	errno == EAGAIN ? "too many processes" : strerror(errno));
    if (noexit)
	return(0);
    exit(1);
}

#else
# ifdef HAVE_SETREUID

/*
 * Set real and effective uids and gids based on perm.
 * We always retain a real or effective uid of ROOT_UID unless
 * we are headed for an exec().
 * This version of set_perms() works fine with the "stay_setuid" option.
 */
int
set_perms(perm)
    int perm;
{
    const char *errstr;
    int noexit;

    noexit = ISSET(perm, PERM_NOEXIT);
    CLR(perm, PERM_MASK);

    if (perm == current_perm)
	return(1);

    switch (perm) {
	case PERM_ROOT:
				if (setreuid(-1, ROOT_UID)) {
				    errstr = "setreuid(-1, ROOT_UID)";
				    goto bad;
				}
				if (setuid(ROOT_UID)) {
				    errstr = "setuid(ROOT_UID)";
				    goto bad;
				}
				(void) setregid(-1, user_gid);
				if (current_perm == PERM_RUNAS)
				    restore_groups();
			      	break;

	case PERM_USER:
    	    	    	        (void) setregid(-1, user_gid);
				if (setreuid(ROOT_UID, user_uid)) {
				    errstr = "setreuid(ROOT_UID, user_uid)";
				    goto bad;
				}
			      	break;
				
	case PERM_FULL_USER:
				/* headed for exec() */
    	    	    	        (void) setgid(user_gid);
				if (setreuid(user_uid, user_uid)) {
				    errstr = "setreuid(user_uid, user_uid)";
				    goto bad;
				}
			      	break;
				
	case PERM_RUNAS:
				runas_setgroups();
				(void) setregid(-1, runas_gr ?
				    runas_gr->gr_gid : runas_pw->pw_gid);
				if (setreuid(-1,
				    runas_pw ? runas_pw->pw_uid : user_uid)) {
				    errstr = "unable to change to runas uid";
				    goto bad;
				}
			      	break;

	case PERM_FULL_RUNAS:
				/* headed for exec(), assume euid == ROOT_UID */
				runas_setup();
				if (setreuid(def_stay_setuid ? user_uid :
				    runas_pw->pw_uid, runas_pw->pw_uid)) {
				    errstr = "unable to change to runas uid";
				    goto bad;
				}
				break;

	case PERM_SUDOERS:
				/* assume euid == ROOT_UID, ruid == user */
				if (setregid(-1, SUDOERS_GID))
				    error(1, "unable to change to sudoers gid");

				/*
				 * If SUDOERS_UID == ROOT_UID and SUDOERS_MODE
				 * is group readable we use a non-zero
				 * uid in order to avoid NFS lossage.
				 * Using uid 1 is a bit bogus but should
				 * work on all OS's.
				 */
				if (SUDOERS_UID == ROOT_UID) {
				    if ((SUDOERS_MODE & 040) && setreuid(ROOT_UID, 1)) {
					errstr = "setreuid(ROOT_UID, 1)";
					goto bad;
				    }
				} else {
				    if (setreuid(ROOT_UID, SUDOERS_UID)) {
					errstr = "setreuid(ROOT_UID, SUDOERS_UID)";
					goto bad;
				    }
				}
			      	break;
	case PERM_TIMESTAMP:
				if (setreuid(ROOT_UID, timestamp_uid)) {
				    errstr = "setreuid(ROOT_UID, timestamp_uid)";
				    goto bad;
				}
			      	break;
    }

    current_perm = perm;
    return(1);
bad:
    warningx("%s: %s", errstr,
	errno == EAGAIN ? "too many processes" : strerror(errno));
    if (noexit)
	return(0);
    exit(1);
}

# else /* !HAVE_SETRESUID && !HAVE_SETREUID */
# ifdef HAVE_SETEUID

/*
 * Set real and effective uids and gids based on perm.
 * NOTE: does not support the "stay_setuid" option.
 */
int
set_perms(perm)
    int perm;
{
    const char *errstr;
    int noexit;

    noexit = ISSET(perm, PERM_NOEXIT);
    CLR(perm, PERM_MASK);

    if (perm == current_perm)
	return(1);

    /*
     * Since we only have setuid() and seteuid() and semantics
     * for these calls differ on various systems, we set
     * real and effective uids to ROOT_UID initially to be safe.
     */
    if (seteuid(ROOT_UID)) {
	errstr = "seteuid(ROOT_UID)";
	goto bad;
    }
    if (setuid(ROOT_UID)) {
	errstr = "setuid(ROOT_UID)";
	goto bad;
    }

    switch (perm) {
	case PERM_ROOT:
				/* uid set above */
				(void) setegid(user_gid);
				if (current_perm == PERM_RUNAS)
				    restore_groups();
			      	break;

	case PERM_USER:
    	    	    	        (void) setegid(user_gid);
				if (seteuid(user_uid)) {
				    errstr = "seteuid(user_uid)";
				    goto bad;
				}
			      	break;
				
	case PERM_FULL_USER:
				/* headed for exec() */
    	    	    	        (void) setgid(user_gid);
				if (setuid(user_uid)) {
				    errstr = "setuid(user_uid)";
				    goto bad;
				}
			      	break;
				
	case PERM_RUNAS:
				runas_setgroups();
				(void) setegid(runas_gr ?
				    runas_gr->gr_gid : runas_pw->pw_gid);
				if (seteuid(runas_pw ? runas_pw->pw_uid : user_uid)) {
				    errstr = "unable to change to runas uid";
				    goto bad;
				}
			      	break;

	case PERM_FULL_RUNAS:
				/* headed for exec() */
				runas_setup();
				if (setuid(runas_pw->pw_uid)) {
				    errstr = "unable to change to runas uid";
				    goto bad;
				}
				break;

	case PERM_SUDOERS:
				if (setegid(SUDOERS_GID))
				    error(1, "unable to change to sudoers gid");

				/*
				 * If SUDOERS_UID == ROOT_UID and SUDOERS_MODE
				 * is group readable we use a non-zero
				 * uid in order to avoid NFS lossage.
				 * Using uid 1 is a bit bogus but should
				 * work on all OS's.
				 */
				if (SUDOERS_UID == ROOT_UID) {
				    if ((SUDOERS_MODE & 040) && seteuid(1)) {
					errstr = "seteuid(1)";
					goto bad;
				    }
				} else {
				    if (seteuid(SUDOERS_UID)) {
					errstr = "seteuid(SUDOERS_UID)";
					goto bad;
				    }
				}
			      	break;
	case PERM_TIMESTAMP:
				if (seteuid(timestamp_uid)) {
				    errstr = "seteuid(timestamp_uid)";
				    goto bad;
				}
			      	break;
    }

    current_perm = perm;
    return(1);
bad:
    warningx("%s: %s", errstr,
	errno == EAGAIN ? "too many processes" : strerror(errno));
    if (noexit)
	return(0);
    exit(1);
}

# else /* !HAVE_SETRESUID && !HAVE_SETREUID && !HAVE_SETEUID */

/*
 * Set uids and gids based on perm via setuid() and setgid().
 * NOTE: does not support the "stay_setuid" or timestampowner options.
 *       Also, SUDOERS_UID and SUDOERS_GID are not used.
 */
int
set_perms(perm)
    int perm;
{
    const char *errstr;
    int noexit;

    noexit = ISSET(perm, PERM_NOEXIT);
    CLR(perm, PERM_MASK);

    if (perm == current_perm)
	return(1);

    switch (perm) {
	case PERM_ROOT:
				if (setuid(ROOT_UID)) {
				    errstr = "setuid(ROOT_UID)";
				    goto bad;
				}
				if (current_perm == PERM_RUNAS)
				    restore_groups();
				break;

	case PERM_FULL_USER:
    	    	    	        (void) setgid(user_gid);
				if (setuid(user_uid)) {
				    errstr = "setuid(user_uid)";
				    goto bad;
				}
			      	break;
				
	case PERM_FULL_RUNAS:
				runas_setup();
				if (setuid(runas_pw->pw_uid)) {
				    errstr = "unable to change to runas uid";
				    goto bad;
				}
				break;

	case PERM_USER:
	case PERM_SUDOERS:
	case PERM_RUNAS:
	case PERM_TIMESTAMP:
				/* Unsupported since we can't set euid. */
				break;
    }

    current_perm = perm;
    return(1);
bad:
    warningx("%s: %s", errstr,
	errno == EAGAIN ? "too many processes" : strerror(errno));
    if (noexit)
	return(0);
    exit(1);
}
#  endif /* HAVE_SETEUID */
# endif /* HAVE_SETREUID */
#endif /* HAVE_SETRESUID */

#ifdef HAVE_INITGROUPS
static void
runas_setgroups()
{
    static int ngroups = -1;
#ifdef HAVE_GETGROUPS
    static GETGROUPS_T *groups;
#endif
    struct passwd *pw;

    if (def_preserve_groups)
	return;

    /*
     * Use stashed copy of runas groups if available, else initgroups and stash.
     */
    if (ngroups == -1) {
	pw = runas_pw ? runas_pw : sudo_user.pw;
	if (initgroups(pw->pw_name, pw->pw_gid) < 0)
	    log_error(USE_ERRNO|MSG_ONLY, "can't set runas group vector");
#ifdef HAVE_GETGROUPS
	if ((ngroups = getgroups(0, NULL)) > 0) {
	    groups = emalloc2(ngroups, sizeof(GETGROUPS_T));
	    if (getgroups(ngroups, groups) < 0)
		log_error(USE_ERRNO|MSG_ONLY, "can't get runas group vector");
	}
    } else {
	if (setgroups(ngroups, groups) < 0)
	    log_error(USE_ERRNO|MSG_ONLY, "can't set runas group vector");
#endif /* HAVE_GETGROUPS */
    }
}

static void
restore_groups()
{
    if (setgroups(user_ngroups, user_groups) < 0)
	log_error(USE_ERRNO|MSG_ONLY, "can't reset user group vector");
}

#else

static void
runas_setgroups()
{
    /* STUB */
}

static void
restore_groups()
{
    /* STUB */
}

#endif /* HAVE_INITGROUPS */

static void
runas_setup()
{
    gid_t gid;
#ifdef HAVE_LOGIN_CAP_H
    int flags;
    extern login_cap_t *lc;
#endif

    if (runas_pw->pw_name != NULL) {
	gid = runas_gr ? runas_gr->gr_gid : runas_pw->pw_gid;
#ifdef HAVE_GETUSERATTR
	aix_setlimits(runas_pw->pw_name);
#endif
#ifdef HAVE_PAM
	pam_prep_user(runas_pw);
#endif /* HAVE_PAM */

#ifdef HAVE_LOGIN_CAP_H
	if (def_use_loginclass) {
	    /*
             * We only use setusercontext() to set the nice value and rlimits.
	     */
	    flags = LOGIN_SETRESOURCES|LOGIN_SETPRIORITY;
	    if (setusercontext(lc, runas_pw, runas_pw->pw_uid, flags)) {
		if (runas_pw->pw_uid != ROOT_UID)
		    error(1, "unable to set user context");
		else
		    warning("unable to set user context");
	    }
	}
#endif /* HAVE_LOGIN_CAP_H */
	/*
	 * Initialize group vector
	 */
	runas_setgroups();
#ifdef HAVE_SETEUID
	if (setegid(gid))
	    warning("cannot set egid to runas gid");
#endif
	if (setgid(gid))
	    warning("cannot set gid to runas gid");
    }
}
