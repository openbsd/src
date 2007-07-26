/*
 * Copyright (c) 1999-2005 Todd C. Miller <Todd.Miller@courtesan.com>
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
#ifdef HAVE_ERR_H
# include <err.h>
#else
# include "emul/err.h"
#endif /* HAVE_ERR_H */
#include <pwd.h>

#include <sdi_athd.h>
#include <sdconf.h>
#include <sdacmvls.h>

#include "sudo.h"
#include "sudo_auth.h"

#ifndef lint
__unused static const char rcsid[] = "$Sudo: securid.c,v 1.12.2.2 2007/06/12 01:28:42 millert Exp $";
#endif /* lint */

union config_record configure;

int
securid_init(pw, promptp, auth)
    struct passwd *pw;
    char **promptp;
    sudo_auth *auth;
{
    static struct SD_CLIENT sd_dat;		/* SecurID data block */

    auth->data = (VOID *) &sd_dat;		/* For method-specific data */

    if (creadcfg() == 0)
	return(AUTH_SUCCESS);
    else
	return(AUTH_FATAL);
}

int
securid_setup(pw, promptp, auth)
    struct passwd *pw;
    char **promptp;
    sudo_auth *auth;
{
    struct SD_CLIENT *sd = (struct SD_CLIENT *) auth->data;

    /* Re-initialize SecurID every time. */
    if (sd_init(sd) == 0) {
	/* The programmer's guide says username is 32 bytes */
	strlcpy(sd->username, pw->pw_name, 32);
	return(AUTH_SUCCESS);
    } else {
	warnx("unable to contact the SecurID server");
	return(AUTH_FATAL);
    }
}

int
securid_verify(pw, pass, auth)
    struct passwd *pw;
    char *pass;
    sudo_auth *auth;
{
    struct SD_CLIENT *sd = (struct SD_CLIENT *) auth->data;
    int rval;

    rval = sd_auth(sd);
    sd_close();
    if (rval == ACM_OK)
	return(AUTH_SUCCESS);
    else
	return(AUTH_FAILURE);
}
