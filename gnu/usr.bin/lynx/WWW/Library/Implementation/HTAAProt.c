
/* MODULE							HTAAProt.c
**		PROTECTION FILE PARSING MODULE
**
** AUTHORS:
**	AL	Ari Luotonen	luotonen@dxcern.cern.ch
**	MD	Mark Donszelmann    duns@vxdeop.cern.ch
**
** HISTORY:
**	20 Oct 93  AL	Now finds uid/gid for nobody/nogroup by name
**			(doesn't use default 65534 right away).
**			Also understands negative uids/gids.
**	14 Nov 93  MD	Added VMS compatibility
**
** BUGS:
**
**
*/

#include "HTUtils.h"

#include <string.h>
#ifndef VMS
#ifndef NOUSERS
#include <pwd.h>	/* Unix password file routine: getpwnam()	*/
#include <grp.h>	/* Unix group file routine: getgrnam()		*/
#endif /* NOUSERS */
#endif /* not VMS */

#include "HTAAUtil.h"
#include "HTAAFile.h"
#include "HTLex.h"	/* Lexical analysor	*/
#include "HTAssoc.h"	/* Association list	*/
#include "HTAAProt.h"	/* Implemented here	*/

#include "LYLeaks.h"

/*
** Protection setup caching
*/
typedef struct {
    char *	prot_filename;
    HTAAProt *	prot;
} HTAAProtCache;

PRIVATE HTList *  prot_cache	= NULL;	/* Protection setup cache.	*/
PRIVATE HTAAProt *default_prot	= NULL;	/* Default protection.		*/
PRIVATE HTAAProt *current_prot	= NULL;	/* Current protection mode	*/
                                        /* which is set up by callbacks	*/
                                        /* from the rule system when	*/
                                        /* a "protect" rule is matched.	*/


/* PRIVATE							isNumber()
**		DOES A CHARACTER STRING REPRESENT A NUMBER
*/
PRIVATE BOOL isNumber ARGS1(CONST char *, s)
{
    CONST char *cur = s;

    if (!s || !*s) return NO;

    if (*cur == '-')
	cur++;		/* Allow initial minus sign in a number */

    while (*cur) {
	if (*cur < '0' || *cur > '9')
	    return NO;
	cur++;
    }
    return YES;
}


#if defined (VMS) || defined (NOUSERS)
/* PUBLIC							HTAA_getUidName()
**		GET THE USER ID NAME (VMS ONLY)
** ON ENTRY:
**	No arguments.
**
** ON EXIT:
**	returns	the user name 
**		Default is "" (nobody).
*/
PUBLIC char * HTAA_getUidName NOARGS
{
    if (current_prot && current_prot->uid_name 
                  && (0 != strcmp(current_prot->uid_name,"nobody")) )
       return(current_prot->uid_name);
    else
       return("");
}

/* PUBLIC							HTAA_getFileName
**		GET THE FILENAME (VMS ONLY)
** ON ENTRY:
**	No arguments.
**
** ON EXIT:
**	returns	the filename
*/
PUBLIC char * HTAA_getFileName NOARGS
{
    if (current_prot && current_prot->filename)
       return(current_prot->filename);
    else
       return("");
}

#else /* not VMS */

/* PUBLIC							HTAA_getUid()
**		GET THE USER ID TO CHANGE THE PROCESS UID TO
** ON ENTRY:
**	No arguments.
**
** ON EXIT:
**	returns	the uid number to give to setuid() system call.
**		Default is 65534 (nobody).
*/
PUBLIC int HTAA_getUid NOARGS
{
    struct passwd *pw = NULL;

    if (current_prot  &&  current_prot->uid_name) {
	if (isNumber(current_prot->uid_name)) {
	    if (NULL != (pw = getpwuid(atoi(current_prot->uid_name)))) {
		if (TRACE) fprintf(stderr, 
				   "%s(%s) returned (%s:%s:%d:%d:...)\n",
				   "HTAA_getUid: getpwuid",
				   current_prot->uid_name,
				   pw->pw_name, pw->pw_passwd,
				   pw->pw_uid, pw->pw_gid);
		return pw->pw_uid;	
	    }
	}
	else {	/* User name (not a number) */
	    if (NULL != (pw = getpwnam(current_prot->uid_name))) {
		if (TRACE) fprintf(stderr, "%s(\"%s\") %s (%s:%s:%d:%d:...)\n",
				   "HTAA_getUid: getpwnam",
				   current_prot->uid_name, "returned",
				   pw->pw_name, pw->pw_passwd,
				   pw->pw_uid, pw->pw_gid);
		return pw->pw_uid;
	    }
	}
    }
    /*
    ** Ok, then let's get uid for nobody.
    */
    if (NULL != (pw = getpwnam("nobody"))) {
	if (TRACE) fprintf(stderr, "HTAA_getUid: Uid for `nobody' is %d\n",
			   pw->pw_uid);
	return pw->pw_uid;
    }
    /*
    ** Ok, then use default.
    */
    return 65534;	/* nobody */
}


/* PUBLIC							HTAA_getGid()
**		GET THE GROUP ID TO CHANGE THE PROCESS GID TO
** ON ENTRY:
**	No arguments.
**
** ON EXIT:
**	returns	the uid number to give to setgid() system call.
**		Default is 65534 (nogroup).
*/
PUBLIC int HTAA_getGid NOARGS
{    
    struct group *gr = NULL;
    
    if (current_prot  &&  current_prot->gid_name) {
	if (isNumber(current_prot->gid_name)) {
	    if (NULL != (gr = getgrgid(atoi(current_prot->gid_name)))) {
#ifndef __EMX__	/* no gr_passwd */
		if (TRACE) fprintf(stderr,
				   "%s(%s) returned (%s:%s:%d:...)\n",
				   "HTAA_getGid: getgrgid",
				   current_prot->gid_name,
				   gr->gr_name, gr->gr_passwd, gr->gr_gid);
#endif
		return gr->gr_gid;
	    }
	}
	else {	/* Group name (not number) */
	    if (NULL != (gr = getgrnam(current_prot->gid_name))) {
#ifndef __EMX__	/* no gr_passwd */
		if (TRACE) fprintf(stderr, 
				   "%s(\"%s\") returned (%s:%s:%d:...)\n",
				   "HTAA_getGid: getgrnam",
				   current_prot->gid_name,
				   gr->gr_name, gr->gr_passwd, gr->gr_gid);
#endif
		return gr->gr_gid;
	    }
	}
    }
    /*
    ** Ok, then let's get gid for nogroup.
    */
    if (NULL != (gr = getgrnam("nogroup"))) {
	if (TRACE) fprintf(stderr, "HTAA_getGid: Gid for `nogroup' is %d\n",
			   gr->gr_gid);
	return gr->gr_gid;
    }
    /*
    ** Ok, then use default.
    */
    return 65534;	/* nogroup */
}
#endif /* not VMS */


/* PRIVATE							HTAA_setIds()
**		SET UID AND GID (AS NAMES OR NUMBERS)
**		TO HTAAProt STRUCTURE
** ON ENTRY:
**	prot		destination.
**	ids		is a string like "james.www" or "1422.69" etc.
**			giving uid and gid.
**
** ON EXIT:
**	returns		nothing.
*/
PRIVATE void HTAA_setIds ARGS2(HTAAProt *,	prot,
			       CONST char *,	ids)
{
    if (ids) {
	char *local_copy = NULL;
	char *point;

	StrAllocCopy(local_copy, ids);
	point = strchr(local_copy, '.');
	if (point) {
	    *(point++) = (char)0;
	    StrAllocCopy(prot->gid_name, point);
	}
	else {
	    StrAllocCopy(prot->gid_name, "nogroup");
	}
	StrAllocCopy(prot->uid_name, local_copy);
	FREE(local_copy);
    }
    else {
	StrAllocCopy(prot->uid_name, "nobody");
	StrAllocCopy(prot->gid_name, "nogroup");
    }
}


/* PRIVATE						HTAA_parseProtFile()
**		PARSE A PROTECTION SETUP FILE AND
**		PUT THE RESULT IN A HTAAProt STRUCTURE
** ON ENTRY:
**	prot		destination structure.
**	fp		open protection file.
**
** ON EXIT:
**	returns		nothing.
*/
PRIVATE void HTAA_parseProtFile ARGS2(HTAAProt *, prot,
				      FILE *,	  fp)
{
    if (prot && fp) {
	LexItem lex_item;
	char *fieldname = NULL;

	while (LEX_EOF != (lex_item = lex(fp))) {

	    while (lex_item == LEX_REC_SEP)	/* Ignore empty lines */
		lex_item = lex(fp);

	    if (lex_item == LEX_EOF)		/* End of file */
		break;

	    if (lex_item == LEX_ALPH_STR) {	/* Valid setup record */
		
		StrAllocCopy(fieldname, HTlex_buffer);
		
		if (LEX_FIELD_SEP != (lex_item = lex(fp)))
		    unlex(lex_item);	/* If someone wants to use colon */
		                        /* after field name it's ok, but */
		                        /* not required. Here we read it.*/

		if (0==strncasecomp(fieldname, "Auth", 4)) {
		    lex_item = lex(fp);
		    while (lex_item == LEX_ALPH_STR) {
			HTAAScheme scheme = HTAAScheme_enum(HTlex_buffer);
			if (scheme != HTAA_UNKNOWN) {
			    if (!prot->valid_schemes)
				prot->valid_schemes = HTList_new();
			    HTList_addObject(prot->valid_schemes,(void*)scheme);
			    if (TRACE) fprintf(stderr, "%s %s `%s'\n",
					       "HTAA_parseProtFile: valid",
					       "authentication scheme:",
					       HTAAScheme_name(scheme));
			}
			else if (TRACE) fprintf(stderr, "%s %s `%s'\n",
						"HTAA_parseProtFile: unknown",
						"authentication scheme:",
						HTlex_buffer);
			
			if (LEX_ITEM_SEP != (lex_item = lex(fp)))
			    break;
			/*
			** Here lex_item == LEX_ITEM_SEP; after item separator
			** it is ok to have one or more newlines (LEX_REC_SEP)
			** and they are ignored (continuation line).
			*/
			do {
			    lex_item = lex(fp);
			} while (lex_item == LEX_REC_SEP);
		    } /* while items in list */
		} /* if "Authenticate" */

		else if (0==strncasecomp(fieldname, "mask", 4)) {
		    prot->mask_group = HTAA_parseGroupDef(fp);
		    lex_item=LEX_REC_SEP; /*groupdef parser read this already*/
		    if (TRACE) {
			if (prot->mask_group) {
			    fprintf(stderr,
				    "HTAA_parseProtFile: Mask group:\n");
			    HTAA_printGroupDef(prot->mask_group);
			} else fprintf(stderr, "HTAA_parseProtFile: %s\n",
				       "Mask group syntax error");
		    }
		} /* if "Mask" */

		else {	/* Just a name-value pair, put it to assoclist */

		    if (LEX_ALPH_STR == (lex_item = lex(fp))) {
			if (!prot->values)
			    prot->values = HTAssocList_new();
			HTAssocList_add(prot->values, fieldname, HTlex_buffer);
			lex_item = lex(fp);  /* Read record separator */
			if (TRACE) fprintf(stderr, 
					   "%s `%s' bound to value `%s'\n",
					   "HTAA_parseProtFile: Name",
					   fieldname, HTlex_buffer);
		    }
		} /* else name-value pair */

	    } /* if valid field */

	    if (lex_item != LEX_EOF  &&  lex_item != LEX_REC_SEP) {
		if (TRACE) fprintf(stderr, "%s %s %d (that line ignored)\n",
				   "HTAA_parseProtFile: Syntax error",
				   "in protection setup file at line",
				   HTlex_line);
		do {
		    lex_item = lex(fp);
		} while (lex_item != LEX_EOF && lex_item != LEX_REC_SEP);
	    } /* if syntax error */
	} /* while not end-of-file */
	FREE(fieldname);
    } /* if valid parameters */
}


/* PRIVATE						HTAAProt_new()
**		ALLOCATE A NEW HTAAProt STRUCTURE AND
**		INITIALIZE IT FROM PROTECTION SETUP FILE
** ON ENTRY:
**	cur_docname	current filename after rule translations.
**	prot_filename	protection setup file name.
**			If NULL, not an error.
**	ids		Uid and gid names or numbers,
**			examples:
**				james	( <=> james.nogroup)
**				.www	( <=> nobody.www)
**				james.www
**				james.69
**				1422.69
**				1422.www
**
**			May be NULL, defaults to nobody.nogroup.
**			Should be NULL, if prot_file is NULL.
**
** ON EXIT:
**	returns		returns a new and initialized protection
**			setup structure.
**			If setup file is already read in (found
**			in cache), only sets uid_name and gid
**			fields, and returns that.
*/
PRIVATE HTAAProt *HTAAProt_new ARGS3(CONST char *,	cur_docname,
				     CONST char *,	prot_filename,
				     CONST char *,	ids)
{
    HTList *cur = prot_cache;
    HTAAProtCache *cache_item = NULL;
    HTAAProt *prot;
    FILE *fp;

    if (!prot_cache)
	prot_cache = HTList_new();
    
    while (NULL != (cache_item = (HTAAProtCache*)HTList_nextObject(cur))) {
	if (!strcmp(cache_item->prot_filename, prot_filename))
	    break;
    }
    if (cache_item) {
	prot = cache_item->prot;
	if (TRACE) fprintf(stderr, "%s `%s' already in cache\n",
			   "HTAAProt_new: Protection file", prot_filename);
    } else {
	if (TRACE) fprintf(stderr,
			   "HTAAProt_new: Loading protection file `%s'\n",
			   prot_filename);

	if (!(prot = (HTAAProt*)calloc(1, sizeof(HTAAProt))))
	    outofmem(__FILE__, "HTAAProt_new");

	prot->template	= NULL;
	prot->filename	= NULL;
	prot->uid_name	= NULL;
	prot->gid_name	= NULL;
	prot->valid_schemes = HTList_new();
	prot->mask_group= NULL;		/* Masking disabled by defaults */
	prot->values	= HTAssocList_new();

	if (prot_filename && NULL != (fp = fopen(prot_filename, "r"))) {
	    HTAA_parseProtFile(prot, fp);
	    fclose(fp);
	    if (!(cache_item =
	    		(HTAAProtCache*)calloc(1, sizeof(HTAAProtCache))))
		outofmem(__FILE__, "HTAAProt_new");
	    cache_item->prot = prot;
	    cache_item->prot_filename = NULL;
	    StrAllocCopy(cache_item->prot_filename, prot_filename);
	    HTList_addObject(prot_cache, (void*)cache_item);
	}
	else if (TRACE) fprintf(stderr, "HTAAProt_new: %s `%s'\n",
				"Unable to open protection setup file",
				(prot_filename ? prot_filename : "(null)"));
    }

    if (cur_docname)
	StrAllocCopy(prot->filename, cur_docname);
    HTAA_setIds(prot, ids);

    return prot;
}


/* PUBLIC					HTAA_setDefaultProtection()
**		SET THE DEFAULT PROTECTION MODE
**		(called by rule system when a
**		"defprot" rule is matched)
** ON ENTRY:
**	cur_docname	is the current result of rule translations.
**	prot_filename	is the protection setup file (second argument
**			for "defprot" rule, optional)
**	ids		contains user and group names separated by
**			a dot, corresponding to the uid
**			gid under which the server should run,
**			default is "nobody.nogroup" (third argument
**			for "defprot" rule, optional; can be given
**			only if protection setup file is also given).
**
** ON EXIT:
**	returns		nothing.
**			Sets the module-wide variable default_prot.
*/
PUBLIC void HTAA_setDefaultProtection ARGS3(CONST char *,	cur_docname,
					    CONST char *,	prot_filename,
					    CONST char *,	ids)
{
    default_prot = NULL;	/* Not free()'d because this is in cache */

    if (prot_filename) {
	default_prot = HTAAProt_new(cur_docname, prot_filename, ids);
    } else {
	if (TRACE) fprintf(stderr, "%s %s\n",
			   "HTAA_setDefaultProtection: ERROR: Protection file",
			   "not specified (obligatory for DefProt rule)!!\n");
    }
}


/* PUBLIC					HTAA_setCurrentProtection()
**		SET THE CURRENT PROTECTION MODE
**		(called by rule system when a
**		"protect" rule is matched)
** ON ENTRY:
**	cur_docname	is the current result of rule translations.
**	prot_filename	is the protection setup file (second argument
**			for "protect" rule, optional)
**	ids		contains user and group names separated by
**			a dot, corresponding to the uid
**			gid under which the server should run,
**			default is "nobody.nogroup" (third argument
**			for "protect" rule, optional; can be given
**			only if protection setup file is also given).
**
** ON EXIT:
**	returns		nothing.
**			Sets the module-wide variable current_prot.
*/
PUBLIC void HTAA_setCurrentProtection ARGS3(CONST char *,	cur_docname,
					    CONST char *,	prot_filename,
					    CONST char *,	ids)
{
    current_prot = NULL;	/* Not free()'d because this is in cache */

    if (prot_filename) {
	current_prot = HTAAProt_new(cur_docname, prot_filename, ids);
    } else {
	if (default_prot) {
	    current_prot = default_prot;
	    HTAA_setIds(current_prot, ids);
	    if (TRACE) fprintf(stderr, "%s %s %s\n",
			       "HTAA_setCurrentProtection: Protection file",
			       "not specified for Protect rule",
			       "-- using default protection");
	} else {
	    if (TRACE) fprintf(stderr, "%s %s %s\n",
			       "HTAA_setCurrentProtection: ERROR: Protection",
			       "file not specified for Protect rule, and",
			       "default protection is not set!!");
	}
    }
}


/* PUBLIC					HTAA_getCurrentProtection()
**		GET CURRENT PROTECTION SETUP STRUCTURE
**		(this is set up by callbacks made from
**		 the rule system when matching "protect"
**		 (and "defprot") rules)
** ON ENTRY:
**	HTTranslate() must have been called before calling
**	this function.
**
** ON EXIT:
**	returns	a HTAAProt structure representing the
**		protection setup of the HTTranslate()'d file.
**		This must not be free()'d.
*/
PUBLIC HTAAProt *HTAA_getCurrentProtection NOARGS
{
    return current_prot;
}


/* PUBLIC					HTAA_getDefaultProtection()
**		GET DEFAULT PROTECTION SETUP STRUCTURE
**		AND SET IT TO CURRENT PROTECTION
**		(this is set up by callbacks made from
**		 the rule system when matching "defprot"
**		 rules)
** ON ENTRY:
**	HTTranslate() must have been called before calling
**	this function.
**
** ON EXIT:
**	returns	a HTAAProt structure representing the
**		default protection setup of the HTTranslate()'d
**		file (if HTAA_getCurrentProtection() returned
**		NULL, i.e. if there is no "protect" rule
**		but ACL exists, and we need to know default
**		protection settings).
**		This must not be free()'d.
** IMPORTANT:
**	As a side-effect this tells the protection system that
**	the file is in fact protected and sets the current
**	protection mode to default.
*/
PUBLIC HTAAProt *HTAA_getDefaultProtection NOARGS
{
    if (!current_prot) {
	current_prot = default_prot;
	default_prot = NULL;
    }	
    return current_prot;
}


/* SERVER INTERNAL					HTAA_clearProtections()
**		CLEAR DOCUMENT PROTECTION MODE
**		(ALSO DEFAULT PROTECTION)
**		(called by the rule system)
** ON ENTRY:
**	No arguments.
**
** ON EXIT:
**	returns	nothing.
**		Frees the memory used by protection information.
*/
PUBLIC void HTAA_clearProtections NOARGS
{
    current_prot = NULL;	/* These are not freed because	*/
    default_prot = NULL;	/* they are actually in cache.	*/
}
