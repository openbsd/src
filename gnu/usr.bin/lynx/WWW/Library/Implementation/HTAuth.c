
/* MODULE							HTAuth.c
**			USER AUTHENTICATION
**
** AUTHORS:
**	AL	Ari Luotonen	luotonen@dxcern.cern.ch
**
** HISTORY:
**	AL 14.10.93 Fixed the colon-not-allowed-in-password-bug.
**
** BUGS:
**
**
*/

#include "HTUtils.h"
#include <string.h>
#include "HTPasswd.h"	/* Password file routines	*/
#include "HTAssoc.h"
#include "HTAuth.h"	/* Implemented here		*/
#include "HTUU.h"	/* Uuencoding and uudecoding	*/

#include "LYLeaks.h"

/* PRIVATE					    decompose_auth_string()
**		DECOMPOSE AUTHENTICATION STRING
**		FOR BASIC OR PUBKEY SCHEME
** ON ENTRY:
**	authstring	is the authorization string received
**			from browser.
**
** ON EXIT:
**	returns 	a node representing the user information
**			(as always, this is automatically freed
**			by AA package).
*/
PRIVATE HTAAUser *decompose_auth_string ARGS2(char *,		authstring,
					      HTAAScheme,	scheme)
{
    static HTAAUser *user = NULL;
    static char *cleartext = NULL;
    char *username = NULL;
    char *password = NULL;
    char *i_net_adr = NULL;
    char *timestamp = NULL;
    char *browsers_key = NULL;

    if (!user && !(user = (HTAAUser*)malloc(sizeof(HTAAUser)))) /* Allocated */
	outofmem(__FILE__, "decompose_auth_string");		/* only once */

    user->scheme = scheme;
    user->username = NULL;	/* Not freed, because freeing */
    user->password = NULL;	/* cleartext also frees these */
    user->inet_addr = NULL;	/* See below: ||	      */
    user->timestamp = NULL;	/*	      ||	      */
    user->secret_key = NULL;	/*	      ||	      */
				/*	      \/	      */
    FREE(cleartext);	/* From previous call.				*/
			/* NOTE: parts of this memory are pointed to by */
			/* pointers in HTAAUser structure. Therefore,	*/
			/* this also frees all the strings pointed to	*/
			/* by the static 'user'.			*/

    if (!authstring || !*authstring ||
	scheme != HTAA_BASIC || scheme == HTAA_PUBKEY)
	return NULL;

    if (scheme == HTAA_PUBKEY) {    /* Decrypt authentication string */
	int bytes_decoded;
	char *ciphertext;
	int len = strlen(authstring) + 1;

	if (!(ciphertext = (char*)malloc(len)) ||
	    !(cleartext  = (char*)malloc(len)))
	    outofmem(__FILE__, "decompose_auth_string");

	bytes_decoded = HTUU_decode(authstring,
				    (unsigned char *)ciphertext, len);
	ciphertext[bytes_decoded] = (char)0;
#ifdef PUBKEY
	HTPK_decrypt(ciphertext, cleartext, private_key);
#endif
	FREE(ciphertext);
    }
    else {   /* Just uudecode */
	int bytes_decoded;
	int len = strlen(authstring) + 1;

	if (!(cleartext = (char*)malloc(len)))
	    outofmem(__FILE__, "decompose_auth_string");
	bytes_decoded = HTUU_decode(authstring,
				    (unsigned char *)cleartext, len);
	cleartext[bytes_decoded] = (char)0;
    }


/*
** Extract username and password (for both schemes)
*/
    username = cleartext;
    if (!(password = strchr(cleartext, ':'))) {
	if (TRACE)
	    fprintf(stderr, "%s %s\n",
		    "decompose_auth_string: password field",
		    "missing in authentication string.\n");
	return NULL;
    }
    *(password++) = '\0';

/*
** Extract rest of the fields
*/
    if (scheme == HTAA_PUBKEY) {
	if (			      !(i_net_adr   =strchr(password, ':')) ||
	    (*(i_net_adr++)   ='\0'), !(timestamp   =strchr(i_net_adr,':')) ||
	    (*(timestamp++)   ='\0'), !(browsers_key=strchr(timestamp,':')) ||
	    (*(browsers_key++)='\0')) {

	    if (TRACE) fprintf(stderr, "%s %s\n",
			       "decompose_auth_string: Pubkey scheme",
			       "fields missing in authentication string");
	    return NULL;
	}
    }

/*
** Set the fields into the result
*/
    user->username   = username;
    user->password   = password;
    user->inet_addr  = i_net_adr;
    user->timestamp  = timestamp;
    user->secret_key = browsers_key;

    if (TRACE) {
	if (scheme==HTAA_BASIC)
	    fprintf(stderr, "decompose_auth_string: %s (%s,%s)\n",
		    "Basic scheme authentication string:",
		    username, password);
	else
	    fprintf(stderr, "decompose_auth_string: %s (%s,%s,%s,%s,%s)\n",
		    "Pubkey scheme authentication string:",
		    username, password, i_net_adr, timestamp, browsers_key);
    }

    return user;
}



PRIVATE BOOL HTAA_checkTimeStamp ARGS1(CONST char *, timestamp)
{
    return NO;		/* This is just a stub */
}


PRIVATE BOOL HTAA_checkInetAddress ARGS1(CONST char *, i_net_adr)
{
    return NO;		/* This is just a stub */
}


/* SERVER PUBLIC					HTAA_authenticate()
**			AUTHENTICATE USER
** ON ENTRY:
**	scheme		used authentication scheme.
**	scheme_specifics the scheme specific parameters
**			(authentication string for Basic and
**			Pubkey schemes).
**	prot		is the protection information structure
**			for the file.
**
** ON EXIT:
**	returns 	NULL, if authentication failed.
**			Otherwise a pointer to a structure
**			representing authenticated user,
**			which should not be freed.
*/
PUBLIC HTAAUser *HTAA_authenticate ARGS3(HTAAScheme,	scheme,
					 char *,	scheme_specifics,
					 HTAAProt *,	prot)
{
    if (HTAA_UNKNOWN == scheme || !prot ||
	-1 == HTList_indexOf(prot->valid_schemes, (void*)scheme))
	return NULL;

    switch (scheme) {
      case HTAA_BASIC:
      case HTAA_PUBKEY:
	{
	    HTAAUser *user = decompose_auth_string(scheme_specifics, scheme);
					   /* Remember, user is auto-freed */
	    if (user &&
		HTAA_checkPassword(user->username,
				   user->password,
				   HTAssocList_lookup(prot->values, "passw")) &&
		(HTAA_BASIC == scheme ||
		 (HTAA_checkTimeStamp(user->timestamp) &&
		  HTAA_checkInetAddress(user->inet_addr))))
		return user;
	    else
		return NULL;
	}
	break;
      default:
	/* Other authentication routines go here */
	return NULL;
    }
}

