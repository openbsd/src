/*                                   PASSWORD FILE ROUTINES
                                             
 */

#ifndef HTPASSWD_H
#define HTPASSWD_H

#ifndef HTUTILS_H
#include "HTUtils.h"
#endif /* HTUTILS_H */
#include "HTList.h"

#ifdef SHORT_NAMES
#define HTAAenPw        HTAA_encryptPasswd
#define HTAApwMa        HTAA_passwdMatch
#define HTAAFrPR        HTAAFile_readPasswdRec
#define HTAAchPw        HTAA_checkPasswd
#endif /* SHORT_NAMES */

/*

User Authentication

   HTAA_checkPassword(username,password,passwdfile)opens the password file, and checks if
   the username-password pair is correct. Return value is YES, if and only if they are
   correct. Otherwise, and also if the open fails, returns NO.
   
   If the given password file name is NULL or an empty string, the default password file
   name is used (macro PASSWD_FILE).
   
 */

/* PUBLIC                                               HTAA_checkPassword()
**                      VALIDATE A USERNAME-PASSWORD PAIR
** ON ENTRY:
**      username        is a null-terminated string containing
**                      the client's username.
**      password        is a null-terminated string containing
**                      the client's corresponding password.
**      filename        is a null-terminated absolute filename
**                      for password file.
**                      If NULL or empty, the value of
**                      PASSWD_FILE is used.
** ON EXIT:
**      returns         YES, if the username-password pair was correct.
**                      NO, otherwise; also, if open fails.
*/
PUBLIC BOOL HTAA_checkPassword PARAMS((CONST char * username,
                                       CONST char * password,
                                       CONST char * filename));
/*

Password File Maintenance Routines

 */

/* PUBLIC                                               HTAA_encryptPasswd()
**              ENCRYPT PASSWORD TO THE FORM THAT IT IS SAVED
**              IN THE PASSWORD FILE.
** ON ENTRY:
**      password        is a string of arbitrary lenght.
**
** ON EXIT:
**      returns         password in one-way encrypted form.
**
** NOTE:
**      Uses currently the C library function crypt(), which
**      only accepts at most 8 characters long strings and produces
**      always 13 characters long strings. This function is
**      called repeatedly so that longer strings can be encrypted.
**      This is of course not as safe as encrypting the entire
**      string at once, but then again, we are not that paranoid
**      about the security inside the machine.
**
*/
PUBLIC char *HTAA_encryptPasswd PARAMS((CONST char * password));


/* PUBLIC                                               HTAA_passwdMatch()
**              VERIFY THE CORRECTNESS OF A GIVEN PASSWORD
**              AGAINST A ONE-WAY ENCRYPTED FORM OF PASSWORD.
** ON ENTRY:
**      password        is cleartext password.
**      encrypted       is one-way encrypted password, as returned
**                      by function HTAA_encryptPasswd().
**                      This is typically read from the password
**                      file.
**
** ON EXIT:
**      returns         YES, if password matches the encrypted one.
**                      NO, if not, or if either parameter is NULL.
*/
PUBLIC BOOL HTAA_passwdMatch PARAMS((CONST char * password,
                                     CONST char * encrypted));


/* PUBLIC                                               HTAAFile_readPasswdRec()
**                      READ A RECORD FROM THE PASSWORD FILE
** ON ENTRY:
**      fp              open password file
**      out_username    buffer to put the read username, must be at
**                      least MAX_USERNAME_LEN+1 characters long.
**      out_passwd      buffer to put the read password, must be at
**                      least MAX_PASSWORD_LEN+1 characters long.
** ON EXIT:
**      returns         EOF on end of file,
**                      otherwise the number of read fields
**                      (i.e. in a correct case returns 2).
**      out_username    contains the null-terminated read username.
**      out_password    contains the null-terminated read password.
**
** FORMAT OF PASSWORD FILE:
**      username:password:maybe real name or other stuff
**                              (may include even colons)
**
**      There may be whitespace (blanks or tabs) in the beginning and
**      the end of each field. They are ignored.
*/
PUBLIC int HTAAFile_readPasswdRec PARAMS((FILE * fp,
                                          char * out_username,
                                          char * out_password));
/*

 */

#endif /* not HTPASSWD_H */
/*

   End of file HTPasswd.h.  */
