
/* MODULE							HTACL.c
**		ACCESS CONTROL LIST ROUTINES
**
** AUTHORS:
**	AL	Ari Luotonen	luotonen@dxcern.cern.ch
**	MD	Mark Donszelmann    duns@vxdeop.cern.ch
**
** HISTORY:
**	 8 Nov 93  MD	(VMS only) case insensitive compare reading acl entry, filename
**
**
** BUGS:
**
**
*/


#include "HTUtils.h"

/*#include <stdio.h> included by HTUtils.h -- FM *//* FILE */
#include <string.h>

#include "HTAAFile.h"	/* File routines	*/
#include "HTGroup.h"	/* GroupDef		*/
#include "HTACL.h"	/* Implemented here	*/

#include "LYLeaks.h"

/* PRIVATE						HTAA_getAclFilename()
**	    RESOLVE THE FULL PATHNAME OF ACL FILE FOR A GIVEN FILE
** ON ENTRY:
**	path	is the pathname of the file for which to
**		ACL file should be found.
**
**		ACL filename is computed by replacing
**		the filename by .www_acl in the pathname
**		(this is done to a local copy, of course).
**
** ON EXIT:
**	returns the absolute pathname of ACL file
**		(which is automatically freed next time
**		this fuction is called).
*/
PRIVATE char *HTAA_getAclFilename ARGS1(CONST char *, pathname)
{
    static char * local_copy = NULL;
    static char * acl_path = NULL;
    char * directory = NULL;
    char * filename = NULL;

    StrAllocCopy(local_copy, pathname); /* Also frees local_copy */
					/* from previous call.	 */

    directory = local_copy;
    filename = strrchr(directory, '/');
    if (!filename) {		/* No path in front of filename */
	directory = ".";	/* So use current directory */
	filename = local_copy;	/* and the pathname itself is the filename */
    }
    else {
	*filename = '\0'; /* Truncate filename off from directory path */
	filename++;	  /* and the filename begins from the next character */
    }

    StrAllocCopy(acl_path, directory);	/* Also frees acl_path */
					/* from previous call. */
    StrAllocCat(acl_path, "/");
    StrAllocCat(acl_path, ACL_FILE_NAME);

    return acl_path;
}


/* PUBLIC						HTAA_openAcl()
**		OPEN THE ACL FILE FOR THE GIVEN DOCUMENT
** ON ENTRY:
**	pathname	is the absolute pathname of
**			the file to be accessed.
**
** ON EXIT:
**	returns 	the FILE* to open ACL.
**			NULL, if ACL not found.
*/
PUBLIC FILE *HTAA_openAcl ARGS1(CONST char *, pathname)
{
    return fopen(HTAA_getAclFilename(pathname), "r");
}


/* PUBLIC						HTAA_closeAcl()
**			CLOSE ACL FILE
** ON ENTRY:
**	acl_file is Access Control List file to close.
**
** ON EXIT:
**	returns nothing.
*/
PUBLIC void HTAA_closeAcl ARGS1(FILE *, acl_file)
{
    if (acl_file)  fclose(acl_file);
}


/* PUBLIC						HTAA_getAclEntry()
**			CONSULT THE ACCESS CONTROL LIST AND
**			GIVE A LIST OF GROUPS (AND USERS)
**			AUTHORIZED TO ACCESS A GIVEN FILE
** ON ENTRY:
**	acl_file	is an open ACL file.
**	pathname	is the absolute pathname of
**			the file to be accessed.
**	method		is the method for which access is wanted.
**
** ALC FILE FORMAT:
**
**	template : method, method, ... : group@addr, user, group, ...
**
**	The last item is in fact in exactly the same format as
**	group definition in group file, i.e. everything that
**	follows the 'groupname:' part,
**	e.g.
**		user, group, user@address, group@address,
**		(user,group,...)@(address, address, ...)
**
** ON EXIT:
**	returns 	NULL, if there is no entry for the file in the ACL,
**			or ACL doesn't exist.
**			If there is, a GroupDef object containing the
**			group and user names allowed to access the file
**			is returned (this is automatically freed
**			next time this function is called).
** IMPORTANT:
**	Returns the first entry with matching template and
**	method. This function should be called multiple times
**	to process all the valid entries (until it returns NULL).
**	This is because there can be multiple entries like:
**
**		*.html : get,put : ari,timbl,robert
**		*.html : get	 : jim,james,jonathan,jojo
**
** NOTE:
**	The returned group definition may well contain references
**	to groups defined in group file. Therefore these references
**	must be resolved according to that rule file by function
**	HTAA_resolveGroupReferences() (group file is read in by
**	HTAA_readGroupFile()) and after that access authorization
**	can be checked with function HTAA_userAndInetGroup().
*/
PUBLIC GroupDef *HTAA_getAclEntry ARGS3(FILE *, 	acl_file,
					CONST char *,	pathname,
					HTAAMethod,	method)
{
    static GroupDef * group_def = NULL;
    CONST char * filename;
    int len;
    char *buf;

    if (!acl_file) return NULL; 	/* ACL doesn't exist */

    if (group_def) {
	GroupDef_delete(group_def);	/* From previous call */
	group_def = NULL;
    }

    if (!(filename = strrchr(pathname, '/')))
	filename = pathname;
    else filename++;	/* Skip slash */

    len = strlen(filename);

    if (!(buf = (char*)malloc((strlen(filename)+2)*sizeof(char))))
	outofmem(__FILE__, "HTAA_getAuthorizedGroups");

    while (EOF != HTAAFile_readField(acl_file, buf, len+1)) {
#ifdef VMS
	if (HTAA_templateCaseMatch(buf, filename)) {
#else /* not VMS */
	if (HTAA_templateMatch(buf, filename)) {
#endif /* not VMS */
	    HTList *methods = HTList_new();
	    HTAAFile_readList(acl_file, methods, MAX_METHODNAME_LEN);
	    if (TRACE) {
		fprintf(stderr,
			"Filename '%s' matched template '%s', allowed methods:",
			filename, buf);
	    }
	    if (HTAAMethod_inList(method, methods)) {	/* right method? */
		if (TRACE)
		    fprintf(stderr, " METHOD OK\n");
		HTList_delete(methods);
		methods = NULL;
		FREE(buf);
		group_def = HTAA_parseGroupDef(acl_file);
		/*
		** HTAA_parseGroupDef() already reads the record
		** separator so we don't call HTAAFile_nextRec().
		*/
		return group_def;
	    } else if (TRACE) {
		fprintf(stderr, " METHOD NOT FOUND\n");
	    }
	    HTList_delete(methods);
	    methods = NULL;
	}	/* if template match */
	else {
	    if (TRACE) {
		fprintf(stderr,
			"Filename '%s' didn't match template '%s'\n",
			filename, buf);
	    }
	}

	HTAAFile_nextRec(acl_file);
    }	/* while not eof */
    FREE(buf);

    return NULL;	/* No entry for requested file */
			/* (or an empty entry).        */
}

