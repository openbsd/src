/*                                   PROTECTION SETUP FILE

 */

#ifndef HTAAPROT_H
#define HTAAPROT_H

#include <HTGroup.h>
#include <HTAssoc.h>

#ifdef __cplusplus
extern "C" {
#endif
/*

Server's Representation of Document (Tree) Protections

 */ typedef struct {
	char *ctemplate;	/* Template for this protection         */
	char *filename;		/* Current document file                */
	char *uid_name;		/* Effective uid (name of it)           */
	char *gid_name;		/* Effective gid (name of it)           */
	GroupDef *mask_group;	/* Allowed users and IP addresses       */
	HTList *valid_schemes;	/* Valid authentication schemes         */
	HTAssocList *values;	/* Association list for scheme specific */
	/* parameters.                          */
    } HTAAProt;

/*

Callbacks for rule system

   The following three functioncs are called by the rule system:

      HTAA_clearProtections() when starting to translate a filename

      HTAA_setDefaultProtection() when "defprot" rule is matched

      HTAA_setCurrentProtection() when "protect" rule is matched

   Protection setup files are cached by these functions.

 */

/* PUBLIC                                       HTAA_setDefaultProtection()
 *              SET THE DEFAULT PROTECTION MODE
 *              (called by rule system when a
 *              "defprot" rule is matched)
 * ON ENTRY:
 *      cur_docname     is the current result of rule translations.
 *      prot_filename   is the protection setup file (second argument
 *                      for "defprot" rule, optional)
 *      eff_ids         contains user and group names separated by
 *                      a dot, corresponding to the effective uid
 *                      gid under which the server should run,
 *                      default is "nobody.nogroup" (third argument
 *                      for "defprot" rule, optional; can be given
 *                      only if protection setup file is also given).
 *
 * ON EXIT:
 *      returns         nothing.
 *                      Sets the module-wide variable default_prot.
 */
    extern void HTAA_setDefaultProtection(const char *cur_docname,
					  const char *prot_filename,
					  const char *eff_ids);

/* PUBLIC                                       HTAA_setCurrentProtection()
 *              SET THE CURRENT PROTECTION MODE
 *              (called by rule system when a
 *              "protect" rule is matched)
 * ON ENTRY:
 *      cur_docname     is the current result of rule translations.
 *      prot_filename   is the protection setup file (second argument
 *                      for "protect" rule, optional)
 *      eff_ids         contains user and group names separated by
 *                      a dot, corresponding to the effective uid
 *                      gid under which the server should run,
 *                      default is "nobody.nogroup" (third argument
 *                      for "protect" rule, optional; can be given
 *                      only if protection setup file is also given).
 *
 * ON EXIT:
 *      returns         nothing.
 *                      Sets the module-wide variable current_prot.
 */
    extern void HTAA_setCurrentProtection(const char *cur_docname,
					  const char *prot_filename,
					  const char *eff_ids);

/* SERVER INTERNAL                                      HTAA_clearProtections()
 *              CLEAR DOCUMENT PROTECTION MODE
 *              (ALSO DEFAULT PROTECTION)
 *              (called by the rule system)
 * ON ENTRY:
 *      No arguments.
 *
 * ON EXIT:
 *      returns nothing.
 *              Frees the memory used by protection information.
 */
    extern void HTAA_clearProtections(void);

/*

Getting Protection Settings

      HTAA_getCurrentProtection() returns the current protection mode (if there was a
      "protect" rule). NULL, if no "protect" rule has been matched.

      HTAA_getDefaultProtection() sets the current protection mode to what it was set to
      by "defprot" rule and also returns it (therefore after this call also
      HTAA_getCurrentProtection() returns the same structure.

 */

/* PUBLIC                                       HTAA_getCurrentProtection()
 *              GET CURRENT PROTECTION SETUP STRUCTURE
 *              (this is set up by callbacks made from
 *               the rule system when matching "protect"
 *               (and "defprot") rules)
 * ON ENTRY:
 *      HTTranslate() must have been called before calling
 *      this function.
 *
 * ON EXIT:
 *      returns a HTAAProt structure representing the
 *              protection setup of the HTTranslate()'d file.
 *              This must not be free()'d.
 */
    extern HTAAProt *HTAA_getCurrentProtection(void);

/* PUBLIC                                       HTAA_getDefaultProtection()
 *              GET DEFAULT PROTECTION SETUP STRUCTURE
 *              (this is set up by callbacks made from
 *               the rule system when matching "defprot"
 *               rules)
 * ON ENTRY:
 *      HTTranslate() must have been called before calling
 *      this function.
 *
 * ON EXIT:
 *      returns a HTAAProt structure representing the
 *              default protection setup of the HTTranslate()'d
 *              file (if HTAA_getCurrentProtection() returned
 *              NULL, i.e., if there is no "protect" rule
 *              but ACL exists, and we need to know default
 *              protection settings).
 *              This must not be free()'d.
 */
    extern HTAAProt *HTAA_getDefaultProtection(void);

/*

Get User and Group IDs to Which Set to

 */

#ifndef VMS
/* PUBLIC                                                       HTAA_getUid()
 *              GET THE USER ID TO CHANGE THE PROCESS UID TO
 * ON ENTRY:
 *      No arguments.
 *
 * ON EXIT:
 *      returns the uid number to give to setuid() system call.
 *              Default is 65534 (nobody).
 */
    extern int HTAA_getUid(void);

/* PUBLIC                                                       HTAA_getGid()
 *              GET THE GROUP ID TO CHANGE THE PROCESS GID TO
 * ON ENTRY:
 *      No arguments.
 *
 * ON EXIT:
 *      returns the uid number to give to setgid() system call.
 *              Default is 65534 (nogroup).
 */
    extern int HTAA_getGid(void);
#endif				/* not VMS */
/*

   For VMS:

 */

#ifdef VMS
/* PUBLIC                                                       HTAA_getUidName()
 *              GET THE USER ID NAME (VMS ONLY)
 * ON ENTRY:
 *      No arguments.
 *
 * ON EXIT:
 *      returns the user name
 *              Default is "" (nobody).
 */
    extern const char *HTAA_getUidName(void);

/* PUBLIC                                                       HTAA_getFileName
 *              GET THE FILENAME (VMS ONLY)
 * ON ENTRY:
 *      No arguments.
 *
 * ON EXIT:
 *      returns the filename
 */
    extern const char *HTAA_getFileName(void);
#endif				/* VMS */

/* PUBLIC                                                       HTAA_UidToName
 *              GET THE USER NAME
 * ON ENTRY:
 *      The user-id
 *
 * ON EXIT:
 *      returns the user name
 */
    extern const char *HTAA_UidToName(int uid);

/* PUBLIC                                                       HTAA_NameToUid
 *              GET THE USER ID
 * ON ENTRY:
 *      The user-name
 *
 * ON EXIT:
 *      returns the user id
 */
    extern int HTAA_NameToUid(const char *name);

/* PUBLIC                                                       HTAA_GidToName
 *              GET THE GROUP NAME
 * ON ENTRY:
 *      The group-id
 *
 * ON EXIT:
 *      returns the group name
 */
    extern const char *HTAA_GidToName(int gid);

/* PUBLIC                                                       HTAA_NameToGid
 *              GET THE GROUP ID
 * ON ENTRY:
 *      The group-name
 *
 * ON EXIT:
 *      returns the group id
 */
    extern int HTAA_NameToGid(const char *name);

#ifdef __cplusplus
}
#endif
#endif				/* not HTAAPROT_H */
