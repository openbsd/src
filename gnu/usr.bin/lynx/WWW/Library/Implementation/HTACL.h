/*				  ACCESS CONTROL LIST ROUTINES

 */

#ifndef HTACL_H
#define HTACL_H

#ifndef HTUTILS_H
#include "HTUtils.h"
#endif /* HTUTILS_H */
#include "HTAAUtil.h"
#include "HTGroup.h"

#ifdef SHORT_NAMES
#define HTAAoACL	HTAA_openAcl
#define HTAAcACL	HTAA_closeAcl
#define HTAAgAEn	HTAA_getAclEntry
#endif /* SHORT_NAMES */

/*

Opening Access Control List File

 */

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
PUBLIC FILE *HTAA_openAcl PARAMS((CONST char * pathname));


/* PUBLIC						HTAA_closeAcl()
**			CLOSE ACL FILE
** ON ENTRY:
**	acl_file is Access Control List file to close.
**
** ON EXIT:
**	returns nothing.
*/
PUBLIC void HTAA_closeAcl PARAMS((FILE * acl_file));
/*

Getting ACL Entry

 */

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
PUBLIC GroupDef *HTAA_getAclEntry PARAMS((FILE *	acl_file,
					  CONST char *	pathname,
					  HTAAMethod	method));
/*

 */

#endif /* not HTACL_H */
/*

   End of file HTACL.h.  */
