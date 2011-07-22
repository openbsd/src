/*
 * $LynxId: HTAAProt.c,v 1.31 2009/03/10 00:27:20 tom Exp $
 *
 * MODULE							HTAAProt.c
 *		PROTECTION FILE PARSING MODULE
 *
 * AUTHORS:
 *	AL	Ari Luotonen	luotonen@dxcern.cern.ch
 *	MD	Mark Donszelmann    duns@vxdeop.cern.ch
 *
 * HISTORY:
 *	20 Oct 93  AL	Now finds uid/gid for nobody/nogroup by name
 *			(doesn't use default 65534 right away).
 *			Also understands negative uids/gids.
 *	14 Nov 93  MD	Added VMS compatibility
 *
 * BUGS:
 *
 *
 */

#include <HTUtils.h>

#ifndef VMS
#ifndef NOUSERS
#include <pwd.h>		/* Unix password file routine: getpwnam()       */
#include <grp.h>		/* Unix group file routine: getgrnam()          */
#endif /* NOUSERS */
#endif /* not VMS */

#include <HTAAUtil.h>
#include <HTLex.h>		/* Lexical analysor     */
#include <HTAAProt.h>		/* Implemented here     */

#include <LYUtils.h>
#include <LYLeaks.h>

#define NOBODY    65534		/* -2 in 16-bit environment */
#define NONESUCH  65533		/* -3 in 16-bit environment */

/*
 * Protection setup caching
 */
typedef struct {
    char *prot_filename;
    HTAAProt *prot;
} HTAAProtCache;

static HTList *prot_cache = NULL;	/* Protection setup cache.      */
static HTAAProt *default_prot = NULL;	/* Default protection.          */
static HTAAProt *current_prot = NULL;	/* Current protection mode      */

					/* which is set up by callbacks */
					/* from the rule system when    */
					/* a "protect" rule is matched. */

#ifndef NOUSERS
/* static							isNumber()
 *		DOES A CHARACTER STRING REPRESENT A NUMBER
 */
static BOOL isNumber(const char *s)
{
    const char *cur = s;

    if (isEmpty(s))
	return NO;

    if (*cur == '-')
	cur++;			/* Allow initial minus sign in a number */

    while (*cur) {
	if (*cur < '0' || *cur > '9')
	    return NO;
	cur++;
    }
    return YES;
}

/* PUBLIC							HTAA_getUid()
 *		GET THE USER ID TO CHANGE THE PROCESS UID TO
 * ON ENTRY:
 *	No arguments.
 *
 * ON EXIT:
 *	returns	the uid number to give to setuid() system call.
 *		Default is 65534 (nobody).
 */
int HTAA_getUid(void)
{
    int uid;

    if (current_prot && current_prot->uid_name) {
	if (isNumber(current_prot->uid_name)) {
	    uid = atoi(current_prot->uid_name);
	    if ((*HTAA_UidToName(uid)) != '\0') {
		return uid;
	    }
	} else {		/* User name (not a number) */
	    if ((uid = HTAA_NameToUid(current_prot->uid_name)) != NONESUCH) {
		return uid;
	    }
	}
    }
    /*
     * Ok, then let's get uid for nobody.
     */
    if ((uid = HTAA_NameToUid("nobody")) != NONESUCH) {
	return uid;
    }
    /*
     * Ok, then use default.
     */
    return NOBODY;		/* nobody */
}

/* PUBLIC							HTAA_getGid()
 *		GET THE GROUP ID TO CHANGE THE PROCESS GID TO
 * ON ENTRY:
 *	No arguments.
 *
 * ON EXIT:
 *	returns	the uid number to give to setgid() system call.
 *		Default is 65534 (nogroup).
 */
int HTAA_getGid(void)
{
    int gid;

    if (current_prot && current_prot->gid_name) {
	if (isNumber(current_prot->gid_name)) {
	    gid = atoi(current_prot->gid_name);
	    if (*HTAA_GidToName(gid) != '\0') {
		return gid;
	    }
	} else {		/* Group name (not number) */
	    if ((gid = HTAA_NameToGid(current_prot->gid_name)) != NONESUCH) {
		return gid;
	    }
	}
    }
    /*
     * Ok, then let's get gid for nogroup.
     */
    if ((gid = HTAA_NameToGid("nogroup")) != NONESUCH) {
	return gid;
    }
    /*
     * Ok, then use default.
     */
    return NOBODY;		/* nogroup */
}
#endif /* !NOUSERS */

/* static							HTAA_setIds()
 *		SET UID AND GID (AS NAMES OR NUMBERS)
 *		TO HTAAProt STRUCTURE
 * ON ENTRY:
 *	prot		destination.
 *	ids		is a string like "james.www" or "1422.69" etc.
 *			giving uid and gid.
 *
 * ON EXIT:
 *	returns		nothing.
 */
static void HTAA_setIds(HTAAProt *prot, const char *ids)
{
    if (ids) {
	char *local_copy = NULL;
	char *point;

	StrAllocCopy(local_copy, ids);
	point = strchr(local_copy, '.');
	if (point) {
	    *(point++) = (char) 0;
	    StrAllocCopy(prot->gid_name, point);
	} else {
	    StrAllocCopy(prot->gid_name, "nogroup");
	}
	StrAllocCopy(prot->uid_name, local_copy);
	FREE(local_copy);
    } else {
	StrAllocCopy(prot->uid_name, "nobody");
	StrAllocCopy(prot->gid_name, "nogroup");
    }
}

/* static						HTAA_parseProtFile()
 *		PARSE A PROTECTION SETUP FILE AND
 *		PUT THE RESULT IN A HTAAProt STRUCTURE
 * ON ENTRY:
 *	prot		destination structure.
 *	fp		open protection file.
 *
 * ON EXIT:
 *	returns		nothing.
 */
static void HTAA_parseProtFile(HTAAProt *prot, FILE *fp)
{
    if (prot && fp) {
	LexItem lex_item;
	char *fieldname = NULL;

	while (LEX_EOF != (lex_item = lex(fp))) {

	    while (lex_item == LEX_REC_SEP)	/* Ignore empty lines */
		lex_item = lex(fp);

	    if (lex_item == LEX_EOF)	/* End of file */
		break;

	    if (lex_item == LEX_ALPH_STR) {	/* Valid setup record */

		StrAllocCopy(fieldname, HTlex_buffer);

		if (LEX_FIELD_SEP != (lex_item = lex(fp)))
		    unlex(lex_item);	/* If someone wants to use colon */
		/* after field name it's ok, but */
		/* not required. Here we read it. */

		if (0 == strncasecomp(fieldname, "Auth", 4)) {
		    lex_item = lex(fp);
		    while (lex_item == LEX_ALPH_STR) {
			HTAAScheme scheme = HTAAScheme_enum(HTlex_buffer);

			if (scheme != HTAA_UNKNOWN) {
			    if (!prot->valid_schemes)
				prot->valid_schemes = HTList_new();
			    HTList_addObject(prot->valid_schemes, (void *) scheme);
			    CTRACE((tfp, "%s %s `%s'\n",
				    "HTAA_parseProtFile: valid",
				    "authentication scheme:",
				    HTAAScheme_name(scheme)));
			} else {
			    CTRACE((tfp, "%s %s `%s'\n",
				    "HTAA_parseProtFile: unknown",
				    "authentication scheme:",
				    HTlex_buffer));
			}

			if (LEX_ITEM_SEP != (lex_item = lex(fp)))
			    break;
			/*
			 * Here lex_item == LEX_ITEM_SEP; after item separator
			 * it is ok to have one or more newlines (LEX_REC_SEP)
			 * and they are ignored (continuation line).
			 */
			do {
			    lex_item = lex(fp);
			} while (lex_item == LEX_REC_SEP);
		    }		/* while items in list */
		}
		/* if "Authenticate" */
		else if (0 == strncasecomp(fieldname, "mask", 4)) {
		    prot->mask_group = HTAA_parseGroupDef(fp);
		    lex_item = LEX_REC_SEP;	/*groupdef parser read this already */
		    if (TRACE) {
			if (prot->mask_group) {
			    fprintf(tfp,
				    "HTAA_parseProtFile: Mask group:\n");
			    HTAA_printGroupDef(prot->mask_group);
			} else
			    fprintf(tfp,
				    "HTAA_parseProtFile: Mask group syntax error\n");
		    }
		}
		/* if "Mask" */
		else {		/* Just a name-value pair, put it to assoclist */

		    if (LEX_ALPH_STR == (lex_item = lex(fp))) {
			if (!prot->values)
			    prot->values = HTAssocList_new();
			HTAssocList_add(prot->values, fieldname, HTlex_buffer);
			lex_item = lex(fp);	/* Read record separator */
			CTRACE((tfp, "%s `%s' bound to value `%s'\n",
				"HTAA_parseProtFile: Name",
				fieldname, HTlex_buffer));
		    }
		}		/* else name-value pair */

	    }
	    /* if valid field */
	    if (lex_item != LEX_EOF && lex_item != LEX_REC_SEP) {
		CTRACE((tfp, "%s %s %d (that line ignored)\n",
			"HTAA_parseProtFile: Syntax error",
			"in protection setup file at line",
			HTlex_line));
		do {
		    lex_item = lex(fp);
		} while (lex_item != LEX_EOF && lex_item != LEX_REC_SEP);
	    }			/* if syntax error */
	}			/* while not end-of-file */
	FREE(fieldname);
    }				/* if valid parameters */
}

/* static						HTAAProt_new()
 *		ALLOCATE A NEW HTAAProt STRUCTURE AND
 *		INITIALIZE IT FROM PROTECTION SETUP FILE
 * ON ENTRY:
 *	cur_docname	current filename after rule translations.
 *	prot_filename	protection setup file name.
 *			If NULL, not an error.
 *	ids		Uid and gid names or numbers,
 *			examples:
 *				james	( <=> james.nogroup)
 *				.www	( <=> nobody.www)
 *				james.www
 *				james.69
 *				1422.69
 *				1422.www
 *
 *			May be NULL, defaults to nobody.nogroup.
 *			Should be NULL, if prot_file is NULL.
 *
 * ON EXIT:
 *	returns		returns a new and initialized protection
 *			setup structure.
 *			If setup file is already read in (found
 *			in cache), only sets uid_name and gid
 *			fields, and returns that.
 */
static HTAAProt *HTAAProt_new(const char *cur_docname,
			      const char *prot_filename,
			      const char *ids)
{
    HTList *cur = prot_cache;
    HTAAProtCache *cache_item = NULL;
    HTAAProt *prot;
    FILE *fp;

    if (!prot_cache)
	prot_cache = HTList_new();

    while (NULL != (cache_item = (HTAAProtCache *) HTList_nextObject(cur))) {
	if (!strcmp(cache_item->prot_filename, prot_filename))
	    break;
    }
    if (cache_item) {
	prot = cache_item->prot;
	CTRACE((tfp, "%s `%s' already in cache\n",
		"HTAAProt_new: Protection file", prot_filename));
    } else {
	CTRACE((tfp, "HTAAProt_new: Loading protection file `%s'\n",
		prot_filename));

	if ((prot = typecalloc(HTAAProt)) == 0)
	      outofmem(__FILE__, "HTAAProt_new");

	prot->ctemplate = NULL;
	prot->filename = NULL;
	prot->uid_name = NULL;
	prot->gid_name = NULL;
	prot->valid_schemes = HTList_new();
	prot->mask_group = NULL;	/* Masking disabled by defaults */
	prot->values = HTAssocList_new();

	if (prot_filename && NULL != (fp = fopen(prot_filename, TXT_R))) {
	    HTAA_parseProtFile(prot, fp);
	    fclose(fp);
	    if ((cache_item = typecalloc(HTAAProtCache)) == 0)
		outofmem(__FILE__, "HTAAProt_new");
	    cache_item->prot = prot;
	    cache_item->prot_filename = NULL;
	    StrAllocCopy(cache_item->prot_filename, prot_filename);
	    HTList_addObject(prot_cache, (void *) cache_item);
	} else {
	    CTRACE((tfp, "HTAAProt_new: %s `%s'\n",
		    "Unable to open protection setup file",
		    NONNULL(prot_filename)));
	}
    }

    if (cur_docname)
	StrAllocCopy(prot->filename, cur_docname);
    HTAA_setIds(prot, ids);

    return prot;
}

/* PUBLIC					HTAA_setDefaultProtection()
 *		SET THE DEFAULT PROTECTION MODE
 *		(called by rule system when a
 *		"defprot" rule is matched)
 * ON ENTRY:
 *	cur_docname	is the current result of rule translations.
 *	prot_filename	is the protection setup file (second argument
 *			for "defprot" rule, optional)
 *	ids		contains user and group names separated by
 *			a dot, corresponding to the uid
 *			gid under which the server should run,
 *			default is "nobody.nogroup" (third argument
 *			for "defprot" rule, optional; can be given
 *			only if protection setup file is also given).
 *
 * ON EXIT:
 *	returns		nothing.
 *			Sets the module-wide variable default_prot.
 */
void HTAA_setDefaultProtection(const char *cur_docname,
			       const char *prot_filename,
			       const char *ids)
{
    default_prot = NULL;	/* Not free()'d because this is in cache */

    if (prot_filename) {
	default_prot = HTAAProt_new(cur_docname, prot_filename, ids);
    } else {
	CTRACE((tfp, "%s %s\n",
		"HTAA_setDefaultProtection: ERROR: Protection file",
		"not specified (obligatory for DefProt rule)!!\n"));
    }
}

/* PUBLIC					HTAA_setCurrentProtection()
 *		SET THE CURRENT PROTECTION MODE
 *		(called by rule system when a
 *		"protect" rule is matched)
 * ON ENTRY:
 *	cur_docname	is the current result of rule translations.
 *	prot_filename	is the protection setup file (second argument
 *			for "protect" rule, optional)
 *	ids		contains user and group names separated by
 *			a dot, corresponding to the uid
 *			gid under which the server should run,
 *			default is "nobody.nogroup" (third argument
 *			for "protect" rule, optional; can be given
 *			only if protection setup file is also given).
 *
 * ON EXIT:
 *	returns		nothing.
 *			Sets the module-wide variable current_prot.
 */
void HTAA_setCurrentProtection(const char *cur_docname,
			       const char *prot_filename,
			       const char *ids)
{
    current_prot = NULL;	/* Not free()'d because this is in cache */

    if (prot_filename) {
	current_prot = HTAAProt_new(cur_docname, prot_filename, ids);
    } else {
	if (default_prot) {
	    current_prot = default_prot;
	    HTAA_setIds(current_prot, ids);
	    CTRACE((tfp, "%s %s %s\n",
		    "HTAA_setCurrentProtection: Protection file",
		    "not specified for Protect rule",
		    "-- using default protection"));
	} else {
	    CTRACE((tfp, "%s %s %s\n",
		    "HTAA_setCurrentProtection: ERROR: Protection",
		    "file not specified for Protect rule, and",
		    "default protection is not set!!"));
	}
    }
}

/* PUBLIC					HTAA_getCurrentProtection()
 *		GET CURRENT PROTECTION SETUP STRUCTURE
 *		(this is set up by callbacks made from
 *		 the rule system when matching "protect"
 *		 (and "defprot") rules)
 * ON ENTRY:
 *	HTTranslate() must have been called before calling
 *	this function.
 *
 * ON EXIT:
 *	returns	a HTAAProt structure representing the
 *		protection setup of the HTTranslate()'d file.
 *		This must not be free()'d.
 */
HTAAProt *HTAA_getCurrentProtection(void)
{
    return current_prot;
}

/* PUBLIC					HTAA_getDefaultProtection()
 *		GET DEFAULT PROTECTION SETUP STRUCTURE
 *		AND SET IT TO CURRENT PROTECTION
 *		(this is set up by callbacks made from
 *		 the rule system when matching "defprot"
 *		 rules)
 * ON ENTRY:
 *	HTTranslate() must have been called before calling
 *	this function.
 *
 * ON EXIT:
 *	returns	a HTAAProt structure representing the
 *		default protection setup of the HTTranslate()'d
 *		file (if HTAA_getCurrentProtection() returned
 *		NULL, i.e., if there is no "protect" rule
 *		but ACL exists, and we need to know default
 *		protection settings).
 *		This must not be free()'d.
 * IMPORTANT:
 *	As a side-effect this tells the protection system that
 *	the file is in fact protected and sets the current
 *	protection mode to default.
 */
HTAAProt *HTAA_getDefaultProtection(void)
{
    if (!current_prot) {
	current_prot = default_prot;
	default_prot = NULL;
    }
    return current_prot;
}

/* SERVER INTERNAL					HTAA_clearProtections()
 *		CLEAR DOCUMENT PROTECTION MODE
 *		(ALSO DEFAULT PROTECTION)
 *		(called by the rule system)
 * ON ENTRY:
 *	No arguments.
 *
 * ON EXIT:
 *	returns	nothing.
 *		Frees the memory used by protection information.
 */
void HTAA_clearProtections(void)
{
    current_prot = NULL;	/* These are not freed because  */
    default_prot = NULL;	/* they are actually in cache.  */
}

typedef struct {
    char *name;
    int user;
} USER_DATA;

#ifndef NOUSERS
static HTList *known_grp = NULL;
static HTList *known_pwd = NULL;
static BOOL uidgid_cache_inited = NO;
#endif

#ifdef LY_FIND_LEAKS
static void clear_uidgid_cache(void)
{
#ifndef NOUSERS
    USER_DATA *data;

    if (known_grp) {
	while ((data = HTList_removeLastObject(known_grp)) != NULL) {
	    FREE(data->name);
	    FREE(data);
	}
	FREE(known_grp);
    }
    if (known_pwd) {
	while ((data = HTList_removeLastObject(known_pwd)) != NULL) {
	    FREE(data->name);
	    FREE(data);
	}
	FREE(known_pwd);
    }
#endif
}
#endif /* LY_FIND_LEAKS */

#ifndef NOUSERS
static void save_gid_info(const char *name, int user)
{
    USER_DATA *data = typecalloc(USER_DATA);

    if (!data)
	return;
    if (!known_grp) {
	known_grp = HTList_new();
	if (!uidgid_cache_inited) {
#ifdef LY_FIND_LEAKS
	    atexit(clear_uidgid_cache);
#endif
	    uidgid_cache_inited = YES;
	}
    }
    StrAllocCopy(data->name, name);
    data->user = user;
    HTList_addObject(known_grp, data);
}
#endif /* NOUSERS */

#ifndef NOUSERS
static void save_uid_info(const char *name, int user)
{
    USER_DATA *data = typecalloc(USER_DATA);

    if (!data)
	return;
    if (!known_pwd) {
	known_pwd = HTList_new();
	if (!uidgid_cache_inited) {
#ifdef LY_FIND_LEAKS
	    atexit(clear_uidgid_cache);
#endif
	    uidgid_cache_inited = YES;
	}
    }
    StrAllocCopy(data->name, name);
    data->user = user;
    HTList_addObject(known_pwd, data);
}
#endif /* !NOUSERS */

/* PUBLIC							HTAA_UidToName
 *		GET THE USER NAME
 * ON ENTRY:
 *      The user-id
 *
 * ON EXIT:
 *      returns the user name, or an empty string if not found.
 */
const char *HTAA_UidToName(int uid GCC_UNUSED)
{
#ifndef NOUSERS
    struct passwd *pw;
    HTList *me = known_pwd;

    while (HTList_nextObject(me)) {
	USER_DATA *data = (USER_DATA *) (me->object);

	if (uid == data->user)
	    return data->name;
    }

    if ((pw = getpwuid((uid_t) uid)) != 0
	&& pw->pw_name != 0) {
	CTRACE((tfp, "%s(%d) returned (%s:%d:...)\n",
		"HTAA_UidToName: getpwuid",
		uid,
		pw->pw_name, (int) pw->pw_uid));
	save_uid_info(pw->pw_name, (int) pw->pw_uid);
	return pw->pw_name;
    }
#endif
    return "";
}

/* PUBLIC							HTAA_NameToUid
 *		GET THE USER ID
 * ON ENTRY:
 *      The user-name
 *
 * ON EXIT:
 *      returns the user id, or NONESUCH if not found.
 */
int HTAA_NameToUid(const char *name GCC_UNUSED)
{
#ifndef NOUSERS
    struct passwd *pw;
    HTList *me = known_pwd;

    while (HTList_nextObject(me)) {
	USER_DATA *data = (USER_DATA *) (me->object);

	if (!strcmp(name, data->name))
	    return data->user;
    }

    if ((pw = getpwnam(name)) != 0) {
	CTRACE((tfp, "%s(%s) returned (%s:%d:...)\n",
		"HTAA_NameToUid: getpwnam",
		name,
		pw->pw_name, (int) pw->pw_uid));
	save_uid_info(pw->pw_name, (int) pw->pw_uid);
	return (int) pw->pw_uid;
    }
#endif
    return NONESUCH;
}

/* PUBLIC							HTAA_GidToName
 *		GET THE GROUP NAME
 * ON ENTRY:
 *      The group-id
 *
 * ON EXIT:
 *      returns the group name, or an empty string if not found.
 */
const char *HTAA_GidToName(int gid GCC_UNUSED)
{
#ifndef NOUSERS
    struct group *gr;
    HTList *me = known_grp;

    while (HTList_nextObject(me)) {
	USER_DATA *data = (USER_DATA *) (me->object);

	if (gid == data->user)
	    return data->name;
    }

    if ((gr = getgrgid((gid_t) gid)) != 0
	&& gr->gr_name != 0) {
	CTRACE((tfp, "%s(%d) returned (%s:%d:...)\n",
		"HTAA_GidToName: getgrgid",
		gid,
		gr->gr_name, (int) gr->gr_gid));
	save_gid_info(gr->gr_name, (int) gr->gr_gid);
	return gr->gr_name;
    }
#endif
    return "";
}

/* PUBLIC							HTAA_NameToGid
 *		GET THE GROUP ID
 * ON ENTRY:
 *      The group-name
 *
 * ON EXIT:
 *      returns the group id, or NONESUCH if not found.
 */
int HTAA_NameToGid(const char *name GCC_UNUSED)
{
#ifndef NOUSERS
    struct group *gr;
    HTList *me = known_grp;

    while (HTList_nextObject(me)) {
	USER_DATA *data = (USER_DATA *) (me->object);

	if (!strcmp(name, data->name))
	    return data->user;
    }

    if ((gr = getgrnam(name)) != 0) {
	CTRACE((tfp, "%s(%s) returned (%s:%d:...)\n",
		"HTAA_NameToGid: getgrnam",
		name,
		gr->gr_name, (int) gr->gr_gid));
	save_gid_info(gr->gr_name, (int) gr->gr_gid);
	return (int) gr->gr_gid;
    }
#endif
    return NONESUCH;
}
