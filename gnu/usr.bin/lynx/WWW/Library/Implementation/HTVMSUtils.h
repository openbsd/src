/*             VMS specific routines

 */

#ifndef HTVMSUTIL_H
#define HTVMSUTIL_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#include <HTAnchor.h>

#ifdef __cplusplus
extern "C" {
#endif
    extern BOOL HTVMSFileVersions;	/* Include version numbers in listing? */

/* PUBLIC							HTVMS_authSysPrv()
 *		CHECKS IF THIS PROCESS IS AUTHORIZED TO ENABLE SYSPRV
 * ON ENTRY:
 *	No arguments.
 *
 * ON EXIT:
 *	returns	YES if SYSPRV is authorized
 */
    extern BOOL HTVMS_authSysPrv(void);

/* PUBLIC							HTVMS_enableSysPrv()
 *		ENABLES SYSPRV
 * ON ENTRY:
 *	No arguments.
 *
 * ON EXIT:
 *
 */
    extern void HTVMS_enableSysPrv(void);

/* PUBLIC							HTVMS_disableSysPrv()
 *		DISABLES SYSPRV
 * ON ENTRY:
 *	No arguments.
 *
 * ON EXIT:
 *
 */
    extern void HTVMS_disableSysPrv(void);

/* PUBLIC							HTVMS_checkAccess()
 *		CHECKS ACCESS TO FILE FOR CERTAIN USER
 * ON ENTRY:
 *	FileName	The file to be accessed
 *	UserName	Name of the user to check access for
 *
 * ON EXIT:
 *	returns YES if access is allowed
 *
 */
    extern BOOL HTVMS_checkAccess(const char *FileName,
				  const char *UserName,
				  const char *Method);

/* PUBLIC							HTVMS_wwwName()
 *		CONVERTS VMS Name into WWW Name
 * ON ENTRY:
 *	vmsname		VMS file specification (NO NODE)
 *
 * ON EXIT:
 *	returns		www file specification
 *
 * EXAMPLES:
 *	vmsname				wwwname
 *	DISK$USER			disk$user
 *	DISK$USER:			/disk$user/
 *	DISK$USER:[DUNS]		/disk$user/duns
 *	DISK$USER:[DUNS.ECHO]		/disk$user/duns/echo
 *	[DUNS]				duns
 *	[DUNS.ECHO]			duns/echo
 *	[DUNS.ECHO.-.TRANS]		duns/echo/../trans
 *	[DUNS.ECHO.--.TRANS]		duns/echo/../../trans
 *	[.DUNS]				duns
 *	[.DUNS.ECHO]			duns/echo
 *	[.DUNS.ECHO]TEST.COM		duns/echo/test.com
 *	TEST.COM			test.com
 *
 *
 */
    const extern char *HTVMS_wwwName(const char *vmsname);

    extern int HTVMSBrowseDir(const char *address,
			      HTParentAnchor *anchor,
			      HTFormat format_out,
			      HTStream *sink);

    extern int HTVMS_remove(char *filename);
    extern void HTVMS_purge(char *filename);

#ifdef __cplusplus
}
#endif
#endif				/* not HTVMSUTIL_H */
