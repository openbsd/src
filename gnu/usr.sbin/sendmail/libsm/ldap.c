/*
 * Copyright (c) 2001 Sendmail, Inc. and its suppliers.
 *      All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

#include <sm/gen.h>
SM_RCSID("@(#)$Sendmail: ldap.c,v 1.8 2001/06/06 00:09:47 gshapiro Exp $")

#if LDAPMAP
# include <sys/types.h>
# include <errno.h>
# include <setjmp.h>
# include <stdlib.h>
# include <unistd.h>

# include <sm/bitops.h>
# include <sm/clock.h>
# include <sm/conf.h>
# include <sm/debug.h>
# include <sm/errstring.h>
# include <sm/ldap.h>
# include <sm/string.h>

SM_DEBUG_T SmLDAPTrace = SM_DEBUG_INITIALIZER("sm_trace_ldap",
	"@(#)$Debug: sm_trace_ldap - trace LDAP operations $");

static void	ldaptimeout __P((int));

/*
**  SM_LDAP_CLEAR -- set default values for SM_LDAP_STRUCT
**
**	Parameters:
**		lmap -- pointer to SM_LDAP_STRUCT to clear
**
**	Returns:
**		None.
**
*/

void
sm_ldap_clear(lmap)
	SM_LDAP_STRUCT *lmap;
{
	if (lmap == NULL)
		return;

	lmap->ldap_host = NULL;
	lmap->ldap_port = LDAP_PORT;
	lmap->ldap_deref = LDAP_DEREF_NEVER;
	lmap->ldap_timelimit = LDAP_NO_LIMIT;
	lmap->ldap_sizelimit = LDAP_NO_LIMIT;
# ifdef LDAP_REFERRALS
	lmap->ldap_options = LDAP_OPT_REFERRALS;
# else /* LDAP_REFERRALS */
	lmap->ldap_options = 0;
# endif /* LDAP_REFERRALS */
	lmap->ldap_attrsep = '\0';
	lmap->ldap_binddn = NULL;
	lmap->ldap_secret = NULL;
	lmap->ldap_method = LDAP_AUTH_SIMPLE;
	lmap->ldap_base = NULL;
	lmap->ldap_scope = LDAP_SCOPE_SUBTREE;
	lmap->ldap_attrsonly = LDAPMAP_FALSE;
	lmap->ldap_timeout.tv_sec = 0;
	lmap->ldap_timeout.tv_usec = 0;
	lmap->ldap_ld = NULL;
	lmap->ldap_filter = NULL;
	lmap->ldap_attr[0] = NULL;
	lmap->ldap_res = NULL;
	lmap->ldap_next = NULL;
	lmap->ldap_pid = 0;
}

/*
**  SM_LDAP_START -- actually connect to an LDAP server
**
**	Parameters:
**		name -- name of map for debug output.
**		lmap -- the LDAP map being opened.
**
**	Returns:
**		true if connection is successful, false otherwise.
**
**	Side Effects:
**		Populates lmap->ldap_ld.
*/

static jmp_buf	LDAPTimeout;

#define SM_LDAP_SETTIMEOUT(to)						\
do									\
{									\
	if (to != 0)							\
	{								\
		if (setjmp(LDAPTimeout) != 0)				\
		{							\
			errno = ETIMEDOUT;				\
			return false;					\
		}							\
		ev = sm_setevent(to, ldaptimeout, 0);			\
	}								\
} while (0)

#define SM_LDAP_CLEARTIMEOUT()						\
do									\
{									\
	if (ev != NULL)							\
		sm_clrevent(ev);					\
} while (0)

bool
sm_ldap_start(name, lmap)
	char *name;
	SM_LDAP_STRUCT *lmap;
{
	int bind_result;
	int save_errno;
	SM_EVENT *ev = NULL;
	LDAP *ld;

	if (sm_debug_active(&SmLDAPTrace, 2))
		sm_dprintf("ldapmap_start(%s)\n", name == NULL ? "" : name);

	if (sm_debug_active(&SmLDAPTrace, 9))
		sm_dprintf("ldapmap_start(%s, %d)\n",
			   lmap->ldap_host == NULL ? "localhost" : lmap->ldap_host,
			   lmap->ldap_port);

# if USE_LDAP_INIT
	ld = ldap_init(lmap->ldap_host, lmap->ldap_port);
	save_errno = errno;
# else /* USE_LDAP_INIT */
	/*
	**  If using ldap_open(), the actual connection to the server
	**  happens now so we need the timeout here.  For ldap_init(),
	**  the connection happens at bind time.
	*/

	SM_LDAP_SETTIMEOUT(lmap->ldap_timeout.tv_sec);
	ld = ldap_open(lmap->ldap_host, lmap->ldap_port);
	save_errno = errno;

	/* clear the event if it has not sprung */
	SM_LDAP_CLEARTIMEOUT();
# endif /* USE_LDAP_INIT */

	errno = save_errno;
	if (ld == NULL)
		return false;

	sm_ldap_setopts(ld, lmap);

# if USE_LDAP_INIT
	/*
	**  If using ldap_init(), the actual connection to the server
	**  happens at ldap_bind_s() so we need the timeout here.
	*/

	SM_LDAP_SETTIMEOUT(lmap->ldap_timeout.tv_sec);
# endif /* USE_LDAP_INIT */

# ifdef LDAP_AUTH_KRBV4
	if (lmap->ldap_method == LDAP_AUTH_KRBV4 &&
	    lmap->ldap_secret != NULL)
	{
		/*
		**  Need to put ticket in environment here instead of
		**  during parseargs as there may be different tickets
		**  for different LDAP connections.
		*/

		(void) putenv(lmap->ldap_secret);
	}
# endif /* LDAP_AUTH_KRBV4 */

	bind_result = ldap_bind_s(ld, lmap->ldap_binddn,
				  lmap->ldap_secret, lmap->ldap_method);

# if USE_LDAP_INIT
	/* clear the event if it has not sprung */
	SM_LDAP_CLEARTIMEOUT();
# endif /* USE_LDAP_INIT */

	if (bind_result != LDAP_SUCCESS)
	{
		errno = bind_result + E_LDAPBASE;
		return false;
	}

	/* Save PID to make sure only this PID closes the LDAP connection */
	lmap->ldap_pid = getpid();
	lmap->ldap_ld = ld;
	return true;
}

/* ARGSUSED */
static void
ldaptimeout(unused)
	int unused;
{
	/*
	**  NOTE: THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
	**	ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
	**	DOING.
	*/

	errno = ETIMEDOUT;
	longjmp(LDAPTimeout, 1);
}

/*
**  SM_LDAP_SEARCH -- iniate LDAP search
**
**	Initiate an LDAP search, return the msgid.
**	The calling function must collect the results.
**
**	Parameters:
**		lmap -- LDAP map information
**		key -- key to substitute in LDAP filter
**
**	Returns:
**		-1 on failure, msgid on success
**
*/

int
sm_ldap_search(lmap, key)
	SM_LDAP_STRUCT *lmap;
	char *key;
{
	int msgid;
	char *fp, *p, *q;
	char filter[LDAPMAP_MAX_FILTER + 1];

	/* substitute key into filter, perhaps multiple times */
	memset(filter, '\0', sizeof filter);
	fp = filter;
	p = lmap->ldap_filter;
	while ((q = strchr(p, '%')) != NULL)
	{
		if (q[1] == 's')
		{
			(void) sm_snprintf(fp, SPACELEFT(filter, fp),
					   "%.*s%s", (int) (q - p), p, key);
			fp += strlen(fp);
			p = q + 2;
		}
		else if (q[1] == '0')
		{
			char *k = key;

			(void) sm_snprintf(fp, SPACELEFT(filter, fp),
					   "%.*s", (int) (q - p), p);
			fp += strlen(fp);
			p = q + 2;

			/* Properly escape LDAP special characters */
			while (SPACELEFT(filter, fp) > 0 &&
			       *k != '\0')
			{
				if (*k == '*' || *k == '(' ||
				    *k == ')' || *k == '\\')
				{
					(void) sm_strlcat(fp,
						       (*k == '*' ? "\\2A" :
							(*k == '(' ? "\\28" :
							 (*k == ')' ? "\\29" :
							  (*k == '\\' ? "\\5C" :
							   "\00")))),
						SPACELEFT(filter, fp));
					fp += strlen(fp);
					k++;
				}
				else
					*fp++ = *k++;
			}
		}
		else
		{
			(void) sm_snprintf(fp, SPACELEFT(filter, fp),
				"%.*s", (int) (q - p + 1), p);
			p = q + (q[1] == '%' ? 2 : 1);
			fp += strlen(fp);
		}
	}
	(void) sm_strlcpy(fp, p, SPACELEFT(filter, fp));
	if (sm_debug_active(&SmLDAPTrace, 20))
		sm_dprintf("ldap search filter=%s\n", filter);

	lmap->ldap_res = NULL;
	msgid = ldap_search(lmap->ldap_ld, lmap->ldap_base, lmap->ldap_scope,
			    filter,
			    (lmap->ldap_attr[0] == NULL ? NULL :
			     lmap->ldap_attr),
			    lmap->ldap_attrsonly);
	return msgid;
}

/*
**  SM_LDAP_CLOSE -- close LDAP connection
**
**	Parameters:
**		lmap -- LDAP map information
**
**	Returns:
**		None.
**
*/

void
sm_ldap_close(lmap)
	SM_LDAP_STRUCT *lmap;
{
	if (lmap->ldap_ld == NULL)
		return;

	if (lmap->ldap_pid == getpid())
		ldap_unbind(lmap->ldap_ld);
	lmap->ldap_ld = NULL;
	lmap->ldap_pid = 0;
}

/*
**  SM_LDAP_SETOPTS -- set LDAP options
**
**	Parameters:
**		ld -- LDAP session handle
**		lmap -- LDAP map information
**
**	Returns:
**		None.
**
*/

void
sm_ldap_setopts(ld, lmap)
	LDAP *ld;
	SM_LDAP_STRUCT *lmap;
{
# if USE_LDAP_SET_OPTION
	ldap_set_option(ld, LDAP_OPT_DEREF, &lmap->ldap_deref);
	if (bitset(LDAP_OPT_REFERRALS, lmap->ldap_options))
		ldap_set_option(ld, LDAP_OPT_REFERRALS, LDAP_OPT_ON);
	else
		ldap_set_option(ld, LDAP_OPT_REFERRALS, LDAP_OPT_OFF);
	ldap_set_option(ld, LDAP_OPT_SIZELIMIT, &lmap->ldap_sizelimit);
	ldap_set_option(ld, LDAP_OPT_TIMELIMIT, &lmap->ldap_timelimit);
# else /* USE_LDAP_SET_OPTION */
	/* From here on in we can use ldap internal timelimits */
	ld->ld_deref = lmap->ldap_deref;
	ld->ld_options = lmap->ldap_options;
	ld->ld_sizelimit = lmap->ldap_sizelimit;
	ld->ld_timelimit = lmap->ldap_timelimit;
# endif /* USE_LDAP_SET_OPTION */
}

/*
**  SM_LDAP_GETERRNO -- get ldap errno value
**
**	Parameters:
**		ld -- LDAP session handle
**
**	Returns:
**		LDAP errno.
**
*/

int
sm_ldap_geterrno(ld)
	LDAP *ld;
{
	int err = LDAP_SUCCESS;

# if defined(LDAP_VERSION_MAX) && LDAP_VERSION_MAX >= 3
	(void) ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &err);
# else /* defined(LDAP_VERSION_MAX) && LDAP_VERSION_MAX >= 3 */
#  ifdef LDAP_OPT_SIZELIMIT
	err = ldap_get_lderrno(ld, NULL, NULL);
#  else /* LDAP_OPT_SIZELIMIT */
	err = ld->ld_errno;

	/*
	**  Reset value to prevent lingering LDAP_DECODING_ERROR due to
	**  OpenLDAP 1.X's hack (see above)
	*/

	ld->ld_errno = LDAP_SUCCESS;
#  endif /* LDAP_OPT_SIZELIMIT */
# endif /* defined(LDAP_VERSION_MAX) && LDAP_VERSION_MAX >= 3 */
	return err;
}
# endif /* LDAPMAP */
