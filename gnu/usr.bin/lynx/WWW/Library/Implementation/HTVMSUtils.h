/*             VMS specific routines
                                             
 */

#ifndef HTVMSUTIL_H
#define HTVMSUTIL_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif
 
#include <HTAnchor.h>

extern BOOL HTVMSFileVersions;	/* Include version numbers in listing? */

/* PUBLIC							HTVMS_authSysPrv()
**		CHECKS IF THIS PROCESS IS AUTHORIZED TO ENABLE SYSPRV
** ON ENTRY:
**	No arguments.
**
** ON EXIT:
**	returns	YES if SYSPRV is authorized
*/
PUBLIC BOOL HTVMS_authSysPrv NOPARAMS;


/* PUBLIC							HTVMS_enableSysPrv()
**		ENABLES SYSPRV
** ON ENTRY:
**	No arguments.
**
** ON EXIT:
**	
*/
PUBLIC void HTVMS_enableSysPrv NOPARAMS;


/* PUBLIC							HTVMS_disableSysPrv()
**		DISABLES SYSPRV
** ON ENTRY:
**	No arguments.
**
** ON EXIT:
**	
*/
PUBLIC void HTVMS_disableSysPrv NOPARAMS;

/* PUBLIC							HTVMS_checkAccess()
**		CHECKS ACCESS TO FILE FOR CERTAIN USER
** ON ENTRY:
**	FileName	The file to be accessed
**	UserName	Name of the user to check access for
**
** ON EXIT:
**	returns YES if access is allowed
**	
*/
PUBLIC BOOL HTVMS_checkAccess PARAMS((
	CONST char * FileName,
	CONST char * UserName,
	CONST char * Method));


/* PUBLIC							HTVMS_wwwName()
**		CONVERTS VMS Name into WWW Name 
** ON ENTRY:
**	vmsname		VMS file specification (NO NODE)
**
** ON EXIT:
**	returns 	www file specification
**
** EXAMPLES:
**	vmsname				wwwname
**	DISK$USER 			disk$user
**	DISK$USER: 			/disk$user/
**	DISK$USER:[DUNS] 		/disk$user/duns
**	DISK$USER:[DUNS.ECHO] 		/disk$user/duns/echo
**	[DUNS] 				duns
**	[DUNS.ECHO] 			duns/echo
**	[DUNS.ECHO.-.TRANS] 		duns/echo/../trans
**	[DUNS.ECHO.--.TRANS] 		duns/echo/../../trans
**	[.DUNS] 			duns
**	[.DUNS.ECHO] 			duns/echo
**	[.DUNS.ECHO]TEST.COM 		duns/echo/test.com 
**	TEST.COM 			test.com
**
**	
*/
PUBLIC char * HTVMS_wwwName PARAMS((
	char * vmsname));

/* PUBLIC							HTVMS_name()
**		CONVERTS WWW name into a VMS name
** ON ENTRY:
**	nn		Node Name (optional)
**	fn		WWW file name
**
** ON EXIT:
**	returns 	vms file specification
**
** Bug:	Returns pointer to static -- non-reentrant
*/
PUBLIC char * HTVMS_name PARAMS((
	CONST char * nn, 
	CONST char * fn));

PUBLIC int HTStat PARAMS((
	CONST char * filename,
        struct stat * info));

PUBLIC int HTVMSBrowseDir PARAMS((
	CONST char * address,
	HTParentAnchor * anchor,
	HTFormat format_out,
	HTStream * sink));

extern int HTVMS_remove(char *filename);
extern void HTVMS_purge(char *filename);

#endif /* not HTVMSUTIL_H */
