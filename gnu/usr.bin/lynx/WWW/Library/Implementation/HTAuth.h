/*                                   AUTHENTICATION MODULE
                                             
   This is the authentication module. By modifying the function HTAA_authenticate() it can
   be made to support external authentication methods.
   
 */

#ifndef HTAUTH_H
#define HTAUTH_H

#ifndef HTUTILS_H
#include "HTUtils.h"
#endif /* HTUTILS_H */
#include "HTAAUtil.h"
#include "HTAAProt.h"


#ifdef SHORT_NAMES
#define HTAAauth        HTAA_authenticate
#endif /* SHORT_NAMES */


/*
** Server's representation of a user (fields in authentication string)
*/
typedef struct {
    HTAAScheme  scheme;         /* Scheme used to authenticate this user */
    char *      username;
    char *      password;
    char *      inet_addr;
    char *      timestamp;
    char *      secret_key;
} HTAAUser;
/*

User Authentication

 */

/* SERVER PUBLIC                                        HTAA_authenticate()
**                      AUTHENTICATE USER
** ON ENTRY:
**      scheme          used authentication scheme.
**      scheme_specifics the scheme specific parameters
**                      (authentication string for Basic and
**                      Pubkey schemes).
**      prot            is the protection information structure
**                      for the file.
**
** ON EXIT:
**      returns         NULL, if authentication failed.
**                      Otherwise a pointer to a structure
**                      representing authenticated user,
**                      which should not be freed.
*/
PUBLIC HTAAUser *HTAA_authenticate PARAMS((HTAAScheme   scheme,
                                           char *       scheme_specifics,
                                           HTAAProt *   prot));
/*

 */

#endif /* not HTAUTH_H */
/*

   End of file HTAuth.h.  */
