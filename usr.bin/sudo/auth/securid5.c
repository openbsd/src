/*
 * Copyright (c) 1999-2005, 2007 Todd C. Miller <Todd.Miller@courtesan.com>
 * Copyright (c) 2002 Michael Stroucken <michael@stroucken.org>
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
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include <config.h>

#include <sys/types.h>
#include <sys/param.h>
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

/* Needed for SecurID v5.0 Authentication on UNIX */
#define UNIX 1
#include <acexport.h>
#include <sdacmvls.h>

#include "sudo.h"
#include "sudo_auth.h"

#ifndef lint
__unused static const char rcsid[] = "$Sudo: securid5.c,v 1.13 2008/11/09 14:13:13 millert Exp $";
#endif /* lint */

/*
 * securid_init - Initialises communications with ACE server
 * Arguments in:
 *     pw - UNUSED
 *     promptp - UNUSED
 *     auth - sudo authentication structure
 *
 * Results out:
 *     auth - auth->data contains pointer to new SecurID handle
 *     return code - Fatal if initialization unsuccessful, otherwise
 *                   success.
 */
int
securid_init(pw, promptp, auth)
    struct passwd *pw;
    char **promptp;
    sudo_auth *auth;
{
    static SDI_HANDLE sd_dat;			/* SecurID handle */

    auth->data = (void *) &sd_dat;		/* For method-specific data */

    /* Start communications */
    if (AceInitialize() != SD_FALSE)
	return(AUTH_SUCCESS);

    warningx("failed to initialise the ACE API library");
    return(AUTH_FATAL);
}

/*
 * securid_setup - Initialises a SecurID transaction and locks out other
 *     ACE servers
 *
 * Arguments in:
 *     pw - struct passwd for username
 *     promptp - UNUSED
 *     auth - sudo authentication structure for SecurID handle
 *
 * Results out:
 *     return code - Success if transaction started correctly, fatal
 *                   otherwise
 */
int
securid_setup(pw, promptp, auth)
    struct passwd *pw;
    char **promptp;
    sudo_auth *auth;
{
    SDI_HANDLE *sd = (SDI_HANDLE *) auth->data;
    int retval;

    /* Re-initialize SecurID every time. */
    if (SD_Init(sd) != ACM_OK) {
	warningx("unable to contact the SecurID server");
	return(AUTH_FATAL);
    }

    /* Lock new PIN code */
    retval = SD_Lock(*sd, pw->pw_name);

    switch (retval) {
	case ACM_OK:
		warningx("User ID locked for SecurID Authentication");
		return(AUTH_SUCCESS);

        case ACE_UNDEFINED_USERNAME:
		warningx("invalid username length for SecurID");
		return(AUTH_FATAL);

	case ACE_ERR_INVALID_HANDLE:
		warningx("invalid Authentication Handle for SecurID");
		return(AUTH_FATAL);

	case ACM_ACCESS_DENIED:
		warningx("SecurID communication failed");
		return(AUTH_FATAL);

	default:
		warningx("unknown SecurID error");
		return(AUTH_FATAL);
	}
}

/*
 * securid_verify - Authenticates user and handles ACE responses
 *
 * Arguments in:
 *     pw - struct passwd for username
 *     pass - UNUSED
 *     auth - sudo authentication structure for SecurID handle
 *
 * Results out:
 *     return code - Success on successful authentication, failure on
 *                   incorrect authentication, fatal on errors
 */
int
securid_verify(pw, pass, auth)
    struct passwd *pw;
    char *pass;
    sudo_auth *auth;
{
    SDI_HANDLE *sd = (SDI_HANDLE *) auth->data;
    int rval;

    pass = (char *) tgetpass("Enter your PASSCODE: ",
	def_passwd_timeout * 60, tgetpass_flags);

    /* Have ACE verify password */
    switch (SD_Check(*sd, pass, pw->pw_name)) {
	case ACM_OK:
		rval = AUTH_SUCESS;
		break;

	case ACE_UNDEFINED_PASSCODE:
		warningx("invalid passcode length for SecurID");
		rval = AUTH_FATAL;
		break;

	case ACE_UNDEFINED_USERNAME:
		warningx("invalid username length for SecurID");
		rval = AUTH_FATAL;
		break;

	case ACE_ERR_INVALID_HANDLE:
		warningx("invalid Authentication Handle for SecurID");
		rval = AUTH_FATAL;
		break;

	case ACM_ACCESS_DENIED:
		rval = AUTH_FAILURE;
		break;

	case ACM_NEXT_CODE_REQUIRED:
                /* Sometimes (when current token close to expire?)
                   ACE challenges for the next token displayed
                   (entered without the PIN) */
        	pass = (char *) tgetpass("\
!!! ATTENTION !!!\n\
Wait for the token code to change, \n\
then enter the new token code.\n", \
		def_passwd_timeout * 60, tgetpass_flags);

		if (SD_Next(*sd, pass) == ACM_OK) {
			rval = AUTH_SUCCESS;
			break;
		}

		rval = AUTH_FAILURE;
		break;

	case ACM_NEW_PIN_REQUIRED:
                /*
		 * This user's SecurID has not been activated yet,
                 * or the pin has been reset
		 */
		/* XXX - Is setting up a new PIN within sudo's scope? */
		SD_Pin(*sd, "");
		fprintf(stderr, "Your SecurID access has not yet been set up.\n");
		fprintf(stderr, "Please set up a PIN before you try to authenticate.\n");
		rval = AUTH_FATAL;
		break;

	default:
		warningx("unknown SecurID error");
		rval = AUTH_FATAL;
		break;
    }

    /* Free resources */
    SD_Close(*sd);

    /* Return stored state to calling process */
    return(rval);
}
