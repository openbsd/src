
/* MODULE							HTAABrow.c
**		BROWSER SIDE ACCESS AUTHORIZATION MODULE
**
**	Contains the code for keeping track on server hostnames,
**	port numbers, scheme names, usernames, passwords
**	(and servers' public keys).
**
** IMPORTANT:
**	Routines in this module use dynamic allocation, but free
**	automatically all the memory reserved by them.
**
**	Therefore the caller never has to (and never should)
**	free() any object returned by these functions.
**
**	Therefore also all the strings returned by this package
**	are only valid until the next call to the same function
**	is made.  This approach is selected, because of the nature
**	of access authorization: no string returned by the package
**	needs to be valid longer than until the next call.
**
**	This also makes it easy to plug the AA package in:
**	you don't have to ponder whether to free() something
**	here or is it done somewhere else (because it is always
**	done somewhere else).
**
**	The strings that the package needs to store are copied
**	so the original strings given as parameters to AA
**	functions may be freed or modified with no side effects.
**
**	The AA package does not free() anything else than what
**	it has itself allocated.
**
** AUTHORS:
**	AL	Ari Luotonen	luotonen@dxcern.cern.ch
**
** HISTORY:
**	Oct 17	AL	Made corrections suggested by marca:
**			Added  if (!realm->username) return NULL;
**			Changed some ""s to NULLs.
**			Now doing calloc() to init uuencode source;
**			otherwise HTUU_encode() reads uninitialized memory
**			every now and then (not a real bug but not pretty).
**			Corrected the formula for uuencode destination size.
**
** 28 Apr 1997	AJL	Do Proxy Authorisation.
**
** BUGS:
**
**
*/

#include <HTUtils.h>
#include <HTString.h>
#include <HTParse.h>		/* URL parsing function		*/
#include <HTList.h>		/* HTList object		*/
#include <HTAlert.h>		/* HTConfirm(), HTPrompt()	*/
#include <HTAAUtil.h>		/* AA common to both sides	*/
#include <HTAssoc.h>		/* Assoc list			*/
#include <HTAABrow.h>		/* Implemented here		*/
#include <HTUU.h>		/* Uuencoding and uudecoding	*/

#include <LYLeaks.h>

extern BOOL using_proxy;	/* Are we using an HTTP gateway? */

/*
**  Local datatype definitions
**
**  HTAAServer contains all the information about one server.
*/
typedef struct {

    char *	hostname;	/* Host's name			*/
    int		portnumber;	/* Port number			*/
    BOOL	IsProxy;	/* Is it a proxy?		*/
    HTList *	setups;		/* List of protection setups	*/
                                /* on this server; i.e., valid	*/
                                /* authentication schemes and	*/
                                /* templates when to use them.	*/
                                /* This is actually a list of	*/
                                /* HTAASetup objects.		*/
    HTList *	realms;		/* Information about passwords	*/
} HTAAServer;

/*
**  HTAASetup contains information about one server's one
**  protected tree of documents.
*/
typedef struct {
    HTAAServer *server;		/* Which server serves this tree	     */
    char *	template;	/* Template for this tree		     */
    HTList *	valid_schemes;	/* Valid authentic.schemes   		     */
    HTAssocList**scheme_specifics;/* Scheme specific params		     */
    BOOL	retry;		/* Failed last time -- reprompt (or whatever)*/
} HTAASetup;

/*
**  Information about usernames and passwords in
**  Basic and Pubkey authentication schemes;
*/
typedef struct {
    char *	realmname;	/* Password domain name		*/
    char *	username;	/* Username in that domain	*/
    char *	password;	/* Corresponding password	*/
} HTAARealm;

/*
**  To free off all globals. - FM
*/
PRIVATE void free_HTAAGlobals NOPARAMS;
PRIVATE BOOL free_HTAAGlobalsSet = FALSE;
PRIVATE char *HTAA_composeAuthResult = NULL;
PRIVATE char *compose_auth_stringResult = NULL;	/* Uuencoded presentation */

/*
**  Module-wide global variables
*/
PRIVATE HTList *server_table	= NULL;	/* Browser's info about servers	     */
PRIVATE char *secret_key	= NULL;	/* Browser's latest secret key       */
PRIVATE HTAASetup *current_setup= NULL;	/* The server setup we are currently */
                                        /* talking to			     */
PRIVATE char *current_hostname	= NULL;	/* The server's name and portnumber  */
PRIVATE int current_portnumber	= 80;	/* where we are currently trying to  */
                                        /* connect.			     */
PRIVATE char *current_docname	= NULL;	/* The document's name we are        */
                                        /* trying to access.		     */
PRIVATE char *HTAAForwardAuth	= NULL;	/* Authorization: line to forward    */
                                        /* (used by gateway httpds)	     */
PRIVATE HTAASetup *proxy_setup	= NULL;	/* Same as above, but for Proxy -AJL */
PRIVATE char *proxy_hostname	= NULL;
PRIVATE char *proxy_docname	= NULL;
PRIVATE int proxy_portnumber	= 80;


/*** HTAAForwardAuth for enabling gateway-httpds to forward Authorization ***/

PUBLIC void HTAAForwardAuth_set ARGS2(
	CONST char *,	scheme_name,
	CONST char *,	scheme_specifics)
{
    int len = 20 + (scheme_name      ? strlen(scheme_name)      : 0)
	         + (scheme_specifics ? strlen(scheme_specifics) : 0);

    FREE(HTAAForwardAuth);
    if (!(HTAAForwardAuth = (char*)calloc(1, sizeof(char) * len)))
	outofmem(__FILE__, "HTAAForwardAuth_set");

    strcpy(HTAAForwardAuth, "Authorization: ");
    if (scheme_name) {
	strcat(HTAAForwardAuth, scheme_name);
	strcat(HTAAForwardAuth, " ");
	if (scheme_specifics) {
	    strcat(HTAAForwardAuth, scheme_specifics);
	}
    }
}

PUBLIC void HTAAForwardAuth_reset NOARGS
{
    FREE(HTAAForwardAuth);
}


/**************************** HTAAServer ***********************************/

PRIVATE void HTAASetup_delete PARAMS((HTAASetup * killme));	/* Forward */

/* PRIVATE						HTAAServer_new()
**		ALLOCATE A NEW NODE TO HOLD SERVER INFO
**		AND ADD IT TO THE LIST OF SERVERS
** ON ENTRY:
**	hostname	is the name of the host that the server
**			is running in.
**	portnumber	is the portnumber which the server listens.
**	IsProxy		should be TRUE if this is a proxy.
**
** ON EXIT:
**	returns		the newly-allocated node with all the strings
**			duplicated.
**			Strings will be automatically freed by
**			the function HTAAServer_delete(), which also
**			frees the node itself.
*/
PRIVATE HTAAServer *HTAAServer_new ARGS3(
	CONST char*,	hostname,
	int,		portnumber,
	BOOL,		IsProxy)
{
    HTAAServer *server;

    if (!(server = (HTAAServer *)calloc(1, sizeof(HTAAServer))))
	outofmem(__FILE__, "HTAAServer_new");

    server->hostname	= NULL;
    server->portnumber	= (portnumber > 0 ? portnumber : 80);
    server->IsProxy	= IsProxy;
    server->setups	= HTList_new();
    server->realms	= HTList_new();

    if (hostname)
        StrAllocCopy(server->hostname, hostname);

    if (!server_table)
        server_table = HTList_new();

    HTList_addObject(server_table, (void*)server);

    return server;
}


/* PRIVATE						HTAAServer_delete()
**
**	DELETE THE ENTRY FOR THE SERVER FROM THE HOST TABLE,
**	AND FREE THE MEMORY USED BY IT.
**
** ON ENTRY:
**	killme		points to the HTAAServer to be freed.
**
** ON EXIT:
**	returns		nothing.
*/
PRIVATE void HTAAServer_delete ARGS1(
	HTAAServer *,	killme)
{
    int n, i;
    HTAASetup *setup;
    HTAARealm *realm;
    HTList *cur;

    if (killme) {
	if (killme->setups != NULL) {
	    n = HTList_count(killme->setups);
	    for (i = (n - 1); i >= 0; i--) {
	        if ((setup = (HTAASetup*)HTList_objectAt(killme->setups,
	     						 i)) != NULL) {
		    HTAASetup_delete(setup);
		    setup = NULL;
		}
	    }
	    HTList_delete(killme->setups);
	    killme->setups = NULL;
	}

	cur = killme->realms;
	while (NULL != (realm = (HTAARealm*)HTList_nextObject(cur))) {
	    FREE(realm->realmname);
	    FREE(realm->username);
	    FREE(realm->password);
	    FREE(realm);
	}
	HTList_delete(killme->realms);
	killme->realms = NULL;

	FREE(killme->hostname);

	HTList_removeObject(server_table, (void*)killme);
	FREE(killme);
    }
}

/* PRIVATE						HTAAServer_lookup()
**		LOOK UP SERVER BY HOSTNAME AND PORTNUMBER
** ON ENTRY:
**	hostname	obvious.
**	portnumber	if non-positive defaults to 80.
**	IsProxy		should be TRUE if this is a proxy.
**
**	Looks up the server in the module-global server_table.
**
** ON EXIT:
**	returns		pointer to a HTAAServer structure
**			representing the looked-up server.
**			NULL, if not found.
*/
PRIVATE HTAAServer *HTAAServer_lookup ARGS3(
	CONST char *,	hostname,
	int,		portnumber,
	BOOL,		IsProxy)
{
    if (hostname) {
	HTList *cur = server_table;
	HTAAServer *server;

	if (portnumber <= 0)
	    portnumber = 80;

	while (NULL != (server = (HTAAServer*)HTList_nextObject(cur))) {
	    if (server->portnumber == portnumber  &&
		0==strcmp(server->hostname, hostname) &&
		server->IsProxy == IsProxy)
		return server;
	}
    }
    return NULL;	/* NULL parameter, or not found */
}


/*************************** HTAASetup *******************************/

/* PRIVATE						HTAASetup_lookup()
**	FIGURE OUT WHICH AUTHENTICATION SETUP THE SERVER
**	IS USING FOR A GIVEN FILE ON A GIVEN HOST AND PORT
**
** ON ENTRY:
**	hostname	is the name of the server host machine.
**	portnumber	is the port that the server is running in.
**	docname		is the (URL-)pathname of the document we
**			are trying to access.
**	IsProxy		should be TRUE if this is a proxy.
**
** 	This function goes through the information known about
**	all the setups of the server, and finds out if the given
**	filename resides in one of the protected directories.
**
** ON EXIT:
**	returns		NULL if no match.
**			Otherwise, a HTAASetup structure representing
**			the protected server setup on the corresponding
**			document tree.
**
*/
PRIVATE HTAASetup *HTAASetup_lookup ARGS4(
	CONST char *,	hostname,
	int,		portnumber,
	CONST char *,	docname,
	BOOL,		IsProxy)
{
    HTAAServer *server;
    HTAASetup *setup;

    if (portnumber <= 0)
        portnumber = 80;

    if (hostname && docname && *hostname && *docname &&
	NULL != (server = HTAAServer_lookup(hostname,
					    portnumber,
					    IsProxy))) {

	HTList *cur = server->setups;

	CTRACE(tfp, "%s %s (%s:%d:%s)\n",
		    "HTAASetup_lookup: resolving setup for",
		    (IsProxy ? "proxy" : "server"),
		    hostname, portnumber, docname);

	while (NULL != (setup = (HTAASetup*)HTList_nextObject(cur))) {
	    if (HTAA_templateMatch(setup->template, docname)) {
		CTRACE(tfp, "%s `%s' %s `%s'\n",
			    "HTAASetup_lookup:", docname,
			    "matched template", setup->template);
		return setup;
	    } else {
	        CTRACE(tfp, "%s `%s' %s `%s'\n",
			    "HTAASetup_lookup:", docname,
			    "did NOT match template", setup->template);
	    }
	} /* while setups remain */
    } /* if valid parameters and server found */

    CTRACE(tfp, "%s `%s' %s\n",
		 "HTAASetup_lookup: No template matched",
		 (docname ? docname : "(null)"),
		 "(so probably not protected)");

    return NULL;	/* NULL in parameters, or not found */
}

/* PRIVATE						HTAASetup_new()
**			CREATE A NEW SETUP NODE
** ON ENTRY:
**	server		is a pointer to a HTAAServer structure
**			to which this setup belongs.
**	template	documents matching this template
**			are protected according to this setup.
**	valid_schemes	a list containing all valid authentication
**			schemes for this setup.
**			If NULL, all schemes are disallowed.
**	scheme_specifics is an array of assoc lists, which
**			contain scheme specific parameters given
**			by server in Authenticate: fields.
**			If NULL, all scheme specifics are
**			set to NULL.
** ON EXIT:
**	returns		a new HTAASetup node, and also adds it as
**			part of the HTAAServer given as parameter.
*/
PRIVATE HTAASetup *HTAASetup_new ARGS4(
	HTAAServer *,	server,
	char *,		template,
	HTList *,	valid_schemes,
	HTAssocList **,	scheme_specifics)
{
    HTAASetup *setup;

    if (!server || !template || !*template)
        return NULL;

    if (!(setup = (HTAASetup*)calloc(1, sizeof(HTAASetup))))
	outofmem(__FILE__, "HTAASetup_new");

    setup->retry = NO;
    setup->server = server;
    setup->template = NULL;
    if (template)
        StrAllocCopy(setup->template, template);
    setup->valid_schemes = valid_schemes;
    setup->scheme_specifics = scheme_specifics;

    HTList_addObject(server->setups, (void*)setup);

    return setup;
}

/* PRIVATE						HTAASetup_delete()
**			FREE A HTAASetup STRUCTURE
** ON ENTRY:
**	killme		is a pointer to the structure to free().
**
** ON EXIT:
**	returns		nothing.
*/
PRIVATE void HTAASetup_delete ARGS1(
	HTAASetup *,	killme)
{
    int scheme;

    if (killme) {
	FREE(killme->template);
	if (killme->valid_schemes)
	    HTList_delete(killme->valid_schemes);
	    killme->valid_schemes = NULL;
	for (scheme = 0; scheme < HTAA_MAX_SCHEMES; scheme++)
	    if (killme->scheme_specifics[scheme])
		HTAssocList_delete(killme->scheme_specifics[scheme]);
	FREE(killme->scheme_specifics);
	FREE(killme);
    }
}

/* PRIVATE					HTAASetup_updateSpecifics()
*		COPY SCHEME SPECIFIC PARAMETERS
**		TO HTAASetup STRUCTURE
** ON ENTRY:
**	setup		destination setup structure.
**	specifics	string array containing scheme
**			specific parameters for each scheme.
**			If NULL, all the scheme specific
**			parameters are set to NULL.
**
** ON EXIT:
**	returns		nothing.
*/
PRIVATE void HTAASetup_updateSpecifics ARGS2(
	HTAASetup *,	setup,
	HTAssocList **,	specifics)
{
    int scheme;

    if (setup) {
	if (setup->scheme_specifics) {
	    for (scheme = 0; scheme < HTAA_MAX_SCHEMES; scheme++) {
		if (setup->scheme_specifics[scheme])
		    HTAssocList_delete(setup->scheme_specifics[scheme]);
	    }
	    FREE(setup->scheme_specifics);
	}
	setup->scheme_specifics = specifics;
    }
}


/*************************** HTAARealm **********************************/

/* PRIVATE 						HTAARealm_lookup()
**		LOOKUP HTAARealm STRUCTURE BY REALM NAME
** ON ENTRY:
**	realm_table	a list of realm objects.
**	realmname	is the name of realm to look for.
**
** ON EXIT:
**	returns		the realm.  NULL, if not found.
*/
PRIVATE HTAARealm *HTAARealm_lookup ARGS2(
	HTList *,	realm_table,
	CONST char *,	realmname)
{
    if (realm_table && realmname) {
	HTList *cur = realm_table;
	HTAARealm *realm;

	while (NULL != (realm = (HTAARealm*)HTList_nextObject(cur))) {
	    if (0==strcmp(realm->realmname, realmname))
		return realm;
	}
    }
    return NULL;	/* No table, NULL param, or not found */
}

/* PRIVATE						HTAARealm_new()
**		CREATE A NODE CONTAINING USERNAME AND
**		PASSWORD USED FOR THE GIVEN REALM.
**		IF REALM ALREADY EXISTS, CHANGE
**		USERNAME/PASSWORD.
** ON ENTRY:
**	realm_table	a list of realms to where to add
**			the new one, too.
**	realmname	is the name of the password domain.
**	username	and
**	password	are what you can expect them to be.
**
** ON EXIT:
**	returns		the created realm.
*/
PRIVATE HTAARealm *HTAARealm_new ARGS4(
	HTList *,	realm_table,
	CONST char *,	realmname,
	CONST char *,	username,
	CONST char *,	password)
{
    HTAARealm *realm;

    realm = HTAARealm_lookup(realm_table, realmname);

    if (!realm) {
	if (!(realm = (HTAARealm*)calloc(1, sizeof(HTAARealm))))
	    outofmem(__FILE__, "HTAARealm_new");
	realm->realmname = NULL;
	realm->username = NULL;
	realm->password = NULL;
	StrAllocCopy(realm->realmname, realmname);
	if (realm_table)
	    HTList_addObject(realm_table, (void*)realm);
    }
    if (username)
        StrAllocCopy(realm->username, username);
    if (password)
        StrAllocCopy(realm->password, password);

    return realm;
}


/***************** Basic and Pubkey Authentication ************************/

/* PRIVATE						compose_auth_string()
**
**		COMPOSE Basic OR Pubkey AUTHENTICATION STRING;
**		PROMPTS FOR USERNAME AND PASSWORD IF NEEDED
**
** ON ENTRY:
**	scheme		is either HTAA_BASIC or HTAA_PUBKEY.
**	setup		is the current server setup.
**	IsProxy		should be TRUE if this is a proxy.
**
** ON EXIT:
**	returns		a newly composed authorization string,
**			(with, of course, a newly generated secret
**			key and fresh timestamp, if Pubkey-scheme
**			is being used).
**			NULL, if something fails.
** NOTE:
**	Like throughout the entire AA package, no string or structure
**	returned by AA package needs to (or should) be freed.
**
*/
PRIVATE char *compose_auth_string ARGS3(
	HTAAScheme,	scheme,
	HTAASetup *,	setup,
	BOOL,		IsProxy)
{
    char *cleartext = NULL;	/* Cleartext presentation */
    char *ciphertext = NULL;	/* Encrypted presentation */
    int len;
    char *msg = NULL;
    char *username = NULL;
    char *password = NULL;
    char *realmname = NULL;
    char *theHost = NULL;
    char *proxiedHost = NULL;
    char *thePort = NULL;
    HTAARealm *realm;
    char *i_net_addr = "0.0.0.0";	/* Change... @@@@ */
    char *timestamp = "42";		/* ... these @@@@ */


    FREE(compose_auth_stringResult);	/* From previous call */

    if ((scheme != HTAA_BASIC && scheme != HTAA_PUBKEY) || !setup ||
	!setup->scheme_specifics || !setup->scheme_specifics[scheme] ||
	!setup->server  ||  !setup->server->realms)
	return NULL;

    realmname = HTAssocList_lookup(setup->scheme_specifics[scheme], "realm");
    if (!realmname)
        return NULL;

    realm = HTAARealm_lookup(setup->server->realms, realmname);
    if (!(realm &&
    	  realm->username && *realm->username &&
	  realm->password) || setup->retry) {
	if (!realm) {
	    CTRACE(tfp, "%s `%s' %s\n",
			"compose_auth_string: realm:", realmname,
			"not found -- creating");
	    realm = HTAARealm_new(setup->server->realms,
				  realmname, NULL, NULL);
	}
	/*
	 *  The template should be either the '*' global
	 *  for everything on the server (always true for
	 *  proxy authorization setups), or a path for
	 *  the start of a protected limb, with no host
	 *  field, but we'll check for a host anyway in
	 *  case a WWW-Protection-Template header set an
	 *  absolute URL instead of a path.  If we do get
	 *  a host from this, it will include the port. - FM
	 */
	if ((!IsProxy) && using_proxy && setup->template) {
	    proxiedHost = HTParse(setup->template, "", PARSE_HOST);
	    if (proxiedHost && *proxiedHost != '\0') {
	        theHost = proxiedHost;
	    }
	}
	/*
	 *  If we didn't get a host field from the
	 *  template, set up the host name and port
	 *  from the setup->server elements. - FM
	 */
	if (!theHost)
	    theHost = setup->server->hostname;
	    if (setup->server->portnumber > 0 &&
		setup->server->portnumber != 80) {
		HTSprintf0(&thePort, ":%d", setup->server->portnumber);
	    }
	/*
	 *  Set up the message for the username prompt,
	 *  and then issue the prompt.  The default
	 *  username is included in the call to the
	 *  prompting function, but the password is
	 *  NULL-ed and always replaced. - FM
	 */
	len = strlen(realm->realmname) +
	      strlen(theHost ?
	      	     theHost : "??") + 50;
	HTSprintf0(&msg, gettext("Username for '%s' at %s '%s%s':"),
		     realm->realmname,
		     (IsProxy ? "proxy" : "server"),
		     (theHost ? theHost : "??"),
		     (thePort ? thePort : ""));
	FREE(proxiedHost);
	FREE(thePort);
	username = realm->username;
	password = NULL;
	HTPromptUsernameAndPassword(msg, &username, &password, IsProxy);

	FREE(msg);
	FREE(realm->username);
	FREE(realm->password);
	realm->username = username;
	realm->password = password;

	if (!realm->username || !realm->password) {
	    /*
	     *  Signals to retry. - FM
	     */
	    return NULL;
	} else if (*realm->username == '\0') {
	    /*
	     *  Signals to abort. - FM
	     */
	    StrAllocCopy(compose_auth_stringResult, "");
	    return compose_auth_stringResult;
	}
    }

    len = strlen(realm->username ? realm->username : "") +
	  strlen(realm->password ? realm->password : "") + 3;

    if (scheme == HTAA_PUBKEY) {
#ifdef PUBKEY
	/* Generate new secret key */
	StrAllocCopy(secret_key, HTAA_generateRandomKey());
#endif /* PUBKEY */
	/* Room for secret key, timestamp and inet address */
	len += strlen(secret_key ? secret_key : "") + 30;
    } else {
	FREE(secret_key);
    }

    if (!(cleartext  = (char*)calloc(1, sizeof(char) * len)))
	outofmem(__FILE__, "compose_auth_string");

    if (realm->username)
        strcpy(cleartext, realm->username);
    else
        *cleartext = '\0';

    strcat(cleartext, ":");

    if (realm->password)
        strcat(cleartext, realm->password);

    if (scheme == HTAA_PUBKEY) {
	strcat(cleartext, ":");
	strcat(cleartext, i_net_addr);
	strcat(cleartext, ":");
	strcat(cleartext, timestamp);
	strcat(cleartext, ":");
	if (secret_key)
	    strcat(cleartext, secret_key);

	if (!((ciphertext = (char *)calloc(1, (sizeof(char) * 2) * len)) &&
	      (compose_auth_stringResult =
	      		    (char *)calloc(1, (sizeof(char) * 3) * len))))
	    outofmem(__FILE__, "compose_auth_string");
#ifdef PUBKEY
	HTPK_encrypt(cleartext, ciphertext, server->public_key);
	HTUU_encode((unsigned char *)ciphertext, strlen(ciphertext),
		    compose_auth_stringResult);
#endif /* PUBKEY */
	FREE(cleartext);
	FREE(ciphertext);
    }
    else { /* scheme == HTAA_BASIC */
	if (!(compose_auth_stringResult =
				(char*)calloc(1, (4 * ((len+2)/3)) + 1)))
	    outofmem(__FILE__, "compose_auth_string");
	HTUU_encode((unsigned char *)cleartext, strlen(cleartext),
		    compose_auth_stringResult);
	FREE(cleartext);
    }
    return compose_auth_stringResult;
}

/* BROWSER PRIVATE					HTAA_selectScheme()
**		SELECT THE AUTHENTICATION SCHEME TO USE
** ON ENTRY:
**	setup	is the server setup structure which can
**		be used to make the decision about the
**		used scheme.
**
**	When new authentication methods are added to library
**	this function makes the decision about which one to
**	use at a given time.  This can be done by inspecting
**	environment variables etc.
**
**	Currently only searches for the first valid scheme,
**	and if nothing found suggests Basic scheme;
**
** ON EXIT:
**	returns	the authentication scheme to use.
*/
PRIVATE HTAAScheme HTAA_selectScheme ARGS1(
	HTAASetup *,	setup)
{
    HTAAScheme scheme;

    if (setup && setup->valid_schemes) {
	for (scheme = HTAA_BASIC; scheme < HTAA_MAX_SCHEMES; scheme++)
	    if (-1 < HTList_indexOf(setup->valid_schemes, (void*)scheme))
		return scheme;
    }
    return HTAA_BASIC;
}

/*
**  Purpose:	Free off all module globals.
**  Arguments:	void
**  Return Value:	void
**  Remarks/Portability/Dependencies/Restrictions:
**	To be used at program exit.
**  Revision History:
**	06-19-96	created - FM
*/
PRIVATE void free_HTAAGlobals NOARGS
{
    HTAAServer * server;
    int n, i;

    if (server_table != NULL) {
        n = HTList_count(server_table);
	for (i = (n - 1); i >= 0; i--) {
	    if ((server = (HTAAServer*)HTList_objectAt(server_table,
	     						i)) != NULL) {
	        HTAAServer_delete(server);
		server = NULL;
	    }
	}
	HTList_delete(server_table);
	server_table = NULL;
    }

    HTAAForwardAuth_reset();
    FREE(HTAA_composeAuthResult);
    FREE(current_hostname);
    FREE(current_docname);
    FREE(proxy_hostname);
    FREE(proxy_docname);
    FREE(compose_auth_stringResult);
    FREE(secret_key);
}

/* BROWSER PUBLIC					HTAA_composeAuth()
**
**	SELECT THE AUTHENTICATION SCHEME AND
**	COMPOSE THE ENTIRE AUTHORIZATION HEADER LINE
**	IF WE ALREADY KNOW THAT THE HOST REQUIRES AUTHENTICATION
**
** ON ENTRY:
**	hostname	is the hostname of the server.
**	portnumber	is the portnumber in which the server runs.
**	docname		is the pathname of the document (as in URL)
**	IsProxy		should be TRUE if this is a proxy.
**
** ON EXIT:
**	returns	NULL, if no authorization seems to be needed, or
**		if it is the entire Authorization: line, e.g.
**
**		   "Authorization: Basic username:password"
**
**		As usual, this string is automatically freed.
*/
PUBLIC char *HTAA_composeAuth ARGS4(
	CONST char *,	hostname,
	CONST int,	portnumber,
	CONST char *,	docname,
	BOOL,		IsProxy)
{
    char *auth_string;
    BOOL retry;
    HTAAScheme scheme;
    int len;

    /*
    **  Setup atexit() freeing if not done already. - FM
    */
    if (!free_HTAAGlobalsSet) {
#ifdef LY_FIND_LEAKS
        atexit(free_HTAAGlobals);
#endif
	free_HTAAGlobalsSet = TRUE;
    }

    /*
    ** Make gateway httpds pass authorization field as it was received.
    ** (This still doesn't really work because Authenticate: headers
    **  from remote server are not forwarded to client yet so it cannot
    **  really know that it should send authorization;  I will not
    **  implement it yet because I feel we will soon change radically
    **  the way requests are represented to allow multithreading
    **  on server-side.  Life is hard.)
    */
    if (HTAAForwardAuth) {
	CTRACE(tfp, "HTAA_composeAuth: %s\n",
		    "Forwarding received authorization");
	StrAllocCopy(HTAA_composeAuthResult, HTAAForwardAuth);
	HTAAForwardAuth_reset();	/* Just a precaution */
	return HTAA_composeAuthResult;
    }

    FREE(HTAA_composeAuthResult);	/* From previous call */

    if (IsProxy) {
        /*
	**  Proxy Authorization required. - AJL
	*/

	CTRACE(tfp, "Composing Proxy Authorization for %s:%d/%s\n",
		    hostname, portnumber, docname);

	if (proxy_portnumber != portnumber ||
	    !proxy_hostname || !proxy_docname ||
	    !hostname         || !docname         ||
	    0 != strcmp(proxy_hostname, hostname) ||
	    0 != strcmp(proxy_docname, docname)) {

	    retry = NO;

	    proxy_portnumber = portnumber;

	    if (hostname)
		StrAllocCopy(proxy_hostname, hostname);
	    else
		FREE(proxy_hostname);

	    if (docname)
		StrAllocCopy(proxy_docname, docname);
	    else
		FREE(proxy_docname);
	} else {
	    retry = YES;
	}

	if (!proxy_setup || !retry)
	    proxy_setup = HTAASetup_lookup(hostname, portnumber,
					   docname, IsProxy);

	if (!proxy_setup)
	    return NULL;

    	switch (scheme = HTAA_selectScheme(proxy_setup)) {
	  case HTAA_BASIC:
	  case HTAA_PUBKEY:
	    auth_string = compose_auth_string(scheme, proxy_setup, IsProxy);
	    break;
	  case HTAA_KERBEROS_V4:
	    /* OTHER AUTHENTICATION ROUTINES ARE CALLED HERE */
	  default:
	    {
		char *msg = NULL;
		HTSprintf0(&msg, "%s `%s'",
			gettext("This client doesn't know how to compose proxy authorization information for scheme"),
			HTAAScheme_name(scheme));
		HTAlert(msg);
		FREE(msg);
		auth_string = NULL;
	    }
	} /* switch scheme */

	proxy_setup->retry = NO;

	if (!auth_string)
	    /*
	    **  Signal a failure. - FM
	    */
	    return NULL;  /* Added by marca. */
	if (*auth_string == '\0') {
	    /*
	    **  Signal an abort. - FM
	    */
	    StrAllocCopy(HTAA_composeAuthResult, "");
	    return(HTAA_composeAuthResult);
	}
	len = strlen(auth_string) + strlen((char *)HTAAScheme_name(scheme)) + 26;
	if (!(HTAA_composeAuthResult = (char*)calloc(1, sizeof(char) * len)))
	    outofmem(__FILE__, "HTAA_composeAuth");
	strcpy(HTAA_composeAuthResult, "Proxy-Authorization: ");

    } else {
	/*
	**  Normal WWW authorization.
	*/
	CTRACE(tfp, "Composing Authorization for %s:%d/%s\n",
		    hostname, portnumber, docname);

	if (current_portnumber != portnumber ||
	    !current_hostname || !current_docname ||
	    !hostname         || !docname         ||
	    0 != strcmp(current_hostname, hostname) ||
	    0 != strcmp(current_docname, docname)) {

	    retry = NO;

	    current_portnumber = portnumber;

	    if (hostname)
		StrAllocCopy(current_hostname, hostname);
	    else
		FREE(current_hostname);

	    if (docname)
		StrAllocCopy(current_docname, docname);
	    else
		FREE(current_docname);
	} else {
	    retry = YES;
	}

	if (!current_setup || !retry)
	    current_setup = HTAASetup_lookup(hostname, portnumber,
					     docname, IsProxy);

	if (!current_setup)
	    return NULL;

	switch (scheme = HTAA_selectScheme(current_setup)) {
	  case HTAA_BASIC:
	  case HTAA_PUBKEY:
	    auth_string = compose_auth_string(scheme, current_setup, IsProxy);
	    break;
	  case HTAA_KERBEROS_V4:
	    /* OTHER AUTHENTICATION ROUTINES ARE CALLED HERE */
	  default:
	    {
		char *msg = 0;
		HTSprintf0(&msg, "%s `%s'",
			gettext("This client doesn't know how to compose authorization information for scheme"),
			HTAAScheme_name(scheme));
		HTAlert(msg);
		FREE(msg);
		auth_string = NULL;
	    }
	} /* switch scheme */

	current_setup->retry = NO;

	if (!auth_string)
	    /*
	    **  Signal a failure. - FM
	    */
	    return NULL;  /* Added by marca. */
	if (*auth_string == '\0') {
	    /*
	    **  Signal an abort. - FM
	    */
	    StrAllocCopy(HTAA_composeAuthResult, "");
	    return(HTAA_composeAuthResult);
	}

	len = strlen(auth_string) + strlen((char *)HTAAScheme_name(scheme)) + 20;
	if (!(HTAA_composeAuthResult = (char*)calloc(1, sizeof(char) * len)))
	    outofmem(__FILE__, "HTAA_composeAuth");
	strcpy(HTAA_composeAuthResult, "Authorization: ");
    }

    strcat(HTAA_composeAuthResult, HTAAScheme_name(scheme));
    strcat(HTAA_composeAuthResult, " ");
    strcat(HTAA_composeAuthResult, auth_string);
    return HTAA_composeAuthResult;
}

/* BROWSER PUBLIC				HTAA_shouldRetryWithAuth()
**
**		DETERMINES IF WE SHOULD RETRY THE SERVER
**		WITH AUTHORIZATION
**		(OR IF ALREADY RETRIED, WITH A DIFFERENT
**		USERNAME AND/OR PASSWORD (IF MISSPELLED))
** ON ENTRY:
**	start_of_headers is the first block already read from socket,
**			but status line skipped; i.e., points to the
**			start of the header section.
**	length		is the remaining length of the first block.
**	soc		is the socket to read the rest of server reply.
**	IsProxy		should be TRUE if this is a proxy.
**
**			This function should only be called when
**			server has replied with a 401 (Unauthorized)
**			status code.
** ON EXIT:
**	returns		YES, if connection should be retried.
**			     The node containing all the necessary
**			     information is
**				* either constructed if it does not exist
**				* or password is reset to NULL to indicate
**				  that username and password should be
**				  reprompted when composing Authorization:
**				  field (in function HTAA_composeAuth()).
**			NO, otherwise.
*/
PUBLIC BOOL HTAA_shouldRetryWithAuth ARGS4(
	char *,		start_of_headers,
	int,		length,
	int,		soc,
	BOOL,		IsProxy)
{
    HTAAScheme scheme;
    char *line = NULL;
    int num_schemes = 0;
    HTList *valid_schemes = HTList_new();
    HTAssocList **scheme_specifics = NULL;
    char *template = NULL;
    char *temp = NULL;

    /*
    **  Setup atexit() freeing if not done already. - FM
    */
    if (!free_HTAAGlobalsSet) {
#ifdef LY_FIND_LEAKS
        atexit(free_HTAAGlobals);
#endif
	free_HTAAGlobalsSet = TRUE;
    }

    /*
    **  Read server reply header lines
    */
    CTRACE(tfp, "Server reply header lines:\n");

    HTAA_setupReader(start_of_headers, length, soc);
    while (NULL != (line = HTAA_getUnfoldedLine())  &&  *line != '\0') {
	CTRACE(tfp, "%s\n", line);

	if (strchr(line, ':')) {	/* Valid header line */

	    char *p = line;
	    char *fieldname = HTNextField(&p);
	    char *arg1 = HTNextField(&p);
	    char *args = p;

	    if ((IsProxy &&
		 0==strcasecomp(fieldname, "Proxy-Authenticate:")) ||
	        (!IsProxy &&
		 0==strcasecomp(fieldname, "WWW-Authenticate:"))) {
	        if (!(arg1 && *arg1 && args && *args)) {
		    HTSprintf0(&temp, gettext("Invalid header '%s%s%s%s%s'"), line,
				  ((arg1 && *arg1) ? " "  : ""),
				  ((arg1 && *arg1) ? arg1 : ""),
				  ((args && *args) ? " "  : ""),
				  ((args && *args) ? args : ""));
		    HTAlert(temp);
		    FREE(temp);
		}
		else if (HTAA_UNKNOWN != (scheme = HTAAScheme_enum(arg1))) {
		    HTList_addObject(valid_schemes, (void*)scheme);
		    if (!scheme_specifics) {
			int i;
			scheme_specifics = (HTAssocList**)
			    calloc(1, HTAA_MAX_SCHEMES * sizeof(HTAssocList*));
			if (!scheme_specifics)
			    outofmem(__FILE__, "HTAA_shouldRetryWithAuth");
			for (i=0; i < HTAA_MAX_SCHEMES; i++)
			    scheme_specifics[i] = NULL;
		    }
		    scheme_specifics[scheme] = HTAA_parseArgList(args);
		    num_schemes++;
		} else {
		    CTRACE(tfp, "Unknown scheme `%s' %s\n",
			    (arg1 ? arg1 : "(null)"),
			    (IsProxy ?
			     "in Proxy-Authenticate: field" :
			     "in WWW-Authenticate: field"));
		}
	    }

	    else if (!IsProxy &&
		     0==strcasecomp(fieldname, "WWW-Protection-Template:")) {
		CTRACE(tfp, "Protection template set to `%s'\n", arg1);
		StrAllocCopy(template, arg1);
	    }

	} /* if a valid header line */
	else {
	    CTRACE(tfp, "Invalid header line `%s' ignored\n", line);
	} /* else invalid header line */

	FREE(line);
    } /* while header lines remain */
    FREE(line);


    /*
    **  So should we retry with authorization?
    */
    if (IsProxy) {
	if (num_schemes == 0) {
	    /*
	    **  No proxy authorization valid
	    */
	    proxy_setup = NULL;
	    return NO;
	}
        /*
	**  Doing it for proxy.  -AJL
	*/
	if (proxy_setup && proxy_setup->server) {
	    /*
	    **  We have already tried with proxy authorization.
	    **  Either we don't have access or username or
	    **  password was misspelled.
	    **
	    **  Update scheme-specific parameters
	    **  (in case they have expired by chance).
	    */
	    HTAASetup_updateSpecifics(proxy_setup, scheme_specifics);

	    if (NO == HTConfirm(AUTH_FAILED_PROMPT)) {
		proxy_setup = NULL;
		return NO;
	    } else {
	        /*
		**  Re-ask username+password (if misspelled).
		*/
		proxy_setup->retry = YES;
		return YES;
	    }
	} else {
	    /*
	    **  proxy_setup == NULL, i.e., we have a
	    **  first connection to a protected server or
	    **  the server serves a wider set of documents
	    **  than we expected so far.
	    */
	    HTAAServer *server = HTAAServer_lookup(proxy_hostname,
						   proxy_portnumber,
						   IsProxy);
	    if (!server) {
		server = HTAAServer_new(proxy_hostname,
					proxy_portnumber,
					IsProxy);
	    }
	    if (!template)	/* Proxy matches everything  -AJL */
		StrAllocCopy(template, "*");
	    proxy_setup = HTAASetup_new(server,
					  template,
					  valid_schemes,
					  scheme_specifics);
	    FREE(template);

	    HTAlert(gettext("Proxy authorization required -- retrying"));
	    return YES;
	}
	/* Never reached */
    }
    /*
    **  Normal WWW authorization.
    */
    if (num_schemes == 0) {
	/*
	**  No authorization valid.
	*/
	current_setup = NULL;
	return NO;
    }
    if (current_setup && current_setup->server) {
	/*
	**  So we have already tried with WWW authorization.
	**  Either we don't have access or username or
	**  password was misspelled.
	**
	**  Update scheme-specific parameters
	**  (in case they have expired by chance).
	*/
	HTAASetup_updateSpecifics(current_setup, scheme_specifics);

	if (NO == HTConfirm(AUTH_FAILED_PROMPT)) {
	    current_setup = NULL;
	    return NO;
	} else {
	    /*
	    **  Re-ask username+password (if misspelled).
	    */
	    current_setup->retry = YES;
	    return YES;
	}
    } else {
        /*
	**  current_setup == NULL, i.e., we have a
	**  first connection to a protected server or
	**  the server serves a wider set of documents
	**  than we expected so far.
	*/
	HTAAServer *server = HTAAServer_lookup(current_hostname,
					       current_portnumber,
					       IsProxy);
	if (!server) {
	    server = HTAAServer_new(current_hostname,
				    current_portnumber,
				    IsProxy);
	}
	if (!template)
	    template = HTAA_makeProtectionTemplate(current_docname);
	current_setup = HTAASetup_new(server,
				      template,
				      valid_schemes,
				      scheme_specifics);
	FREE(template);

        HTAlert(gettext("Access without authorization denied -- retrying"));
	return YES;
    }
    /* Never reached */
}

/*
**  This function clears all authorization information by
**  invoking the free_HTAAGlobals() function, which normally
**  is invoked at exit.  It allows a browser command to do
**  this at any time, for example, if the user is leaving
**  the terminal for a period of time, but does not want
**  to end the current session.  - FM
*/
PUBLIC void HTClearHTTPAuthInfo NOARGS
{
    /*
    **  Need code to check cached documents against the
    **  protection templates, and do something to ensure
    **  that any protected documents no longer can be
    **  accessed without a new retrieval. - FM
    */

    /*
    **  Now free all of the authorization info, and
    **  reset the free_HTAAGlobalsSet flag. - FM
    */
    free_HTAAGlobals();
    free_HTAAGlobalsSet = FALSE;
}
