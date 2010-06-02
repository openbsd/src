/*
 * (c) 2006 Quest Software, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of Quest Software, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <dlfcn.h>

#include <vas.h>

#include "compat.h"
#include "logging.h"
#include "nonunix.h"
#include "sudo.h"
#include "parse.h"


/* Pseudo-boolean types */
#undef TRUE
#undef FALSE
#define FALSE 0
#define TRUE  1


static vas_ctx_t *sudo_vas_ctx;
static vas_id_t  *sudo_vas_id;
/* Don't use VAS_NAME_FLAG_NO_CACHE or lookups just won't work.
 * -tedp, 2006-08-29 */
static const int update_flags = 0;
static int sudo_vas_available = 0;
static char *err_msg = NULL;
static void *libvas_handle = NULL;

/* libvas functions */
static vas_err_t	(*v_ctx_alloc) (vas_ctx_t **ctx);
static void		(*v_ctx_free) (vas_ctx_t *ctx);
static vas_err_t	(*v_id_alloc) (vas_ctx_t *ctx, const char *name, vas_id_t **id);
static void		(*v_id_free) (vas_ctx_t *ctx, vas_id_t *id);
static vas_err_t	(*v_id_establish_cred_keytab) (vas_ctx_t *ctx, vas_id_t *id, int credflags, const char *keytab);
static vas_err_t	(*v_user_init) (vas_ctx_t *ctx, vas_id_t *id, const char *name, int flags, vas_user_t **user);
static void		(*v_user_free) (vas_ctx_t *ctx, vas_user_t *user);
static vas_err_t	(*v_group_init) (vas_ctx_t *ctx, vas_id_t *id, const char *name, int flags, vas_group_t **group);
static void		(*v_group_free) (vas_ctx_t *ctx, vas_group_t *group);
static vas_err_t	(*v_user_is_member) (vas_ctx_t *ctx, vas_id_t *id, vas_user_t *user, vas_group_t *group);
static const char*	(*v_err_get_string) (vas_ctx_t *ctx, int with_cause);


static int	resolve_vas_funcs(void);


/**
 * Whether nonunix group lookups are available.
 * @return 1 if available, 0 if not.
 */
int
sudo_nonunix_groupcheck_available(void)
{
    return sudo_vas_available;
}


/**
 * Check if the user is in the group
 * @param group group name which can be in DOMAIN\sam format or just the group
 *              name
 * @param user user name
 * @param pwd (unused)
 * @return 1 if user is a member of the group, 0 if not (or error occurred)
 */
int
sudo_nonunix_groupcheck( const char* group, const char* user, const struct passwd* pwd )
{
    static int          error_cause_shown = FALSE;
    int                 rval = FALSE;
    vas_err_t           vaserr;
    vas_user_t*         vas_user = NULL;
    vas_group_t*        vas_group = NULL;

    if (!sudo_vas_available) {
	if (error_cause_shown == FALSE) {
	    /* Produce the saved error reason */
	    log_error(NO_MAIL|NO_EXIT, "Non-unix group checking unavailable: %s",
		    err_msg ? err_msg
		    : "(unknown cause)");
	    error_cause_shown = TRUE;
	}
	return 0;
    }

    /* resolve the user and group. The user will be a real Unix account name,
     * while the group may be a unix name, or any group name accepted by
     * vas_name_to_dn, which means any of:
     * - Group Name
     * - Group Name@FULLY.QUALIFIED.DOMAIN
     * - CN=sudoers,CN=Users,DC=rcdev,DC=vintela,DC=com
     * - S-1-2-34-5678901234-5678901234-5678901234-567
     *
     * XXX - we may get non-VAS user accounts here. You can add local users to an
     * Active Directory group through override files. Should we handle that case?
     * */
    if( (vaserr = v_user_init( sudo_vas_ctx, sudo_vas_id, user, update_flags, &vas_user )) != VAS_ERR_SUCCESS ) {
	if (vaserr == VAS_ERR_NOT_FOUND) {
	     /* No such user in AD. Probably a local user. */
	    vaserr = VAS_ERR_SUCCESS;
	}
        goto FINISHED;
    }
        
    if( (vaserr = v_group_init( sudo_vas_ctx, sudo_vas_id, group, update_flags, &vas_group )) != VAS_ERR_SUCCESS ) {
        goto FINISHED;
    }

    /* do the membership check */
    if( (vaserr = v_user_is_member( sudo_vas_ctx, sudo_vas_id, vas_user, vas_group )) == VAS_ERR_SUCCESS ) {
        rval = TRUE;
    }
    else if (vaserr == VAS_ERR_NOT_FOUND) {
	/* fake the vaserr code so no error is triggered */
	vaserr = VAS_ERR_SUCCESS;
    }


FINISHED: /* cleanups */
    if (vaserr != VAS_ERR_SUCCESS && vaserr != VAS_ERR_NOT_FOUND ) {
	int error_flags = NO_MAIL | MSG_ONLY | NO_EXIT;

	log_error(error_flags, "Error while checking group membership "
		"for user \"%s\", group \"%s\", error: %s%s.", user, group,
		v_err_get_string(sudo_vas_ctx, 1),
		/* A helpful hint if there seems to be a non-FQDN as the domain */
		(strchr(group, '@') && !strchr(group, '.'))
		 ? "\nMake sure the fully qualified domain name is specified"
		 : "");
    }
    if( vas_group )              v_group_free( sudo_vas_ctx, vas_group );
    if( vas_user )              v_user_free( sudo_vas_ctx, vas_user );

    return(rval);
}


static void
set_err_msg(const char *msg, ...) {
    va_list ap;

    if (!msg) /* assert */
	return;

    if (err_msg)
	free(err_msg);

    va_start(ap, msg);

    if (vasprintf(&err_msg, msg, ap) == -1)
	err_msg = NULL;
	
    va_end(ap);
}


/**
 * Initialise nonunix_groupcheck state.
 */
void
sudo_nonunix_groupcheck_init(void)
{
    vas_err_t vaserr;
    void *libvas;

    if (err_msg) {
	free(err_msg);
	err_msg = NULL;
    }

    libvas = dlopen(LIBVAS_SO, RTLD_LAZY);
    if (!libvas) {
	set_err_msg("dlopen() failed: %s", dlerror());
	return;
    }

    libvas_handle = libvas;

    if (resolve_vas_funcs() != 0)
	return;

    if (VAS_ERR_SUCCESS == (vaserr = v_ctx_alloc(&sudo_vas_ctx))) {

	if (VAS_ERR_SUCCESS == (vaserr = v_id_alloc(sudo_vas_ctx, "host/", &sudo_vas_id))) {
	
	    if (update_flags & VAS_NAME_FLAG_NO_LDAP) {
		sudo_vas_available = 1;
		return; /* OK */
	    } else { /* Get a keytab */
		if ((vaserr = v_id_establish_cred_keytab( sudo_vas_ctx,
						    sudo_vas_id,
						      VAS_ID_FLAG_USE_MEMORY_CCACHE
						    | VAS_ID_FLAG_KEEP_COPY_OF_CRED
						    | VAS_ID_FLAG_NO_INITIAL_TGT,
						    NULL )) == VAS_ERR_SUCCESS) {
		    sudo_vas_available = 1;
		    return; /* OK */
		}

		if (!err_msg)
		    set_err_msg("unable to establish creds: %s",
			    v_err_get_string(sudo_vas_ctx, 1));
	    }

	    v_id_free(sudo_vas_ctx, sudo_vas_id);
	    sudo_vas_id = NULL;
	}

	/* This is the last opportunity to get an error message from libvas */
	if (!err_msg)
	    set_err_msg("Error initializing non-unix group checking: %s",
		    v_err_get_string(sudo_vas_ctx, 1));

	v_ctx_free(sudo_vas_ctx);
	sudo_vas_ctx = NULL;
    }

    if (!err_msg)
	set_err_msg("Failed to get a libvas handle for non-unix group checking (unknown cause)");

    sudo_vas_available = 0;
}


/**
 * Clean up nonunix_groupcheck state.
 */
void
sudo_nonunix_groupcheck_cleanup()
{
    if (err_msg) {
	free(err_msg);
	err_msg = NULL;
    }

    if (sudo_vas_available) {
	v_id_free(sudo_vas_ctx, sudo_vas_id);
	sudo_vas_id = NULL;

	v_ctx_free(sudo_vas_ctx);
	sudo_vas_ctx = NULL;

	sudo_vas_available = FALSE;
    }

    if (libvas_handle) {
	if (dlclose(libvas_handle) != 0)
	    log_error(NO_MAIL|NO_EXIT, "dlclose() failed: %s", dlerror());
	libvas_handle = NULL;
    }
}

#define RESOLVE_OR_ERR(fptr, sym) \
    do { \
	void *_fptr = dlsym(libvas_handle, (sym)); \
	if (!_fptr) { \
	    set_err_msg("dlsym() failed: %s", dlerror()); \
	    return -1; \
	} \
	fptr = _fptr; \
    } while (0)


/**
 * Resolve all the libvas functions.
 * Returns -1 and sets err_msg if something went wrong, or 0 on success.
 */
int
resolve_vas_funcs(void)
{
    if (!libvas_handle) /* assert */
	return -1;

    RESOLVE_OR_ERR(v_ctx_alloc,	"vas_ctx_alloc");
    RESOLVE_OR_ERR(v_ctx_free,	"vas_ctx_free");
    RESOLVE_OR_ERR(v_id_alloc,	"vas_id_alloc");
    RESOLVE_OR_ERR(v_id_free,	"vas_id_free");
    RESOLVE_OR_ERR(v_id_establish_cred_keytab,	"vas_id_establish_cred_keytab");
    RESOLVE_OR_ERR(v_user_init,	"vas_user_init");
    RESOLVE_OR_ERR(v_user_free,	"vas_user_free");
    RESOLVE_OR_ERR(v_group_init,	"vas_group_init");
    RESOLVE_OR_ERR(v_group_free,	"vas_group_free");
    RESOLVE_OR_ERR(v_user_is_member,	"vas_user_is_member");
    RESOLVE_OR_ERR(v_err_get_string,	"vas_err_get_string");

    return 0;
}
