/*	Displaying messages and getting input for LineMode Browser
**	==========================================================
**
**	REPLACE THIS MODULE with a GUI version in a GUI environment!
**
** History:
**	   Jun 92 Created May 1992 By C.T. Barker
**	   Feb 93 Simplified, portablised TBL
**	   Sep 93 Corrected 3 bugs in HTConfirm() :-( AL
*/


#include "HTUtils.h"
#include "tcp.h"		/* for TOUPPER */

#include "HTAlert.h"

#include <ctype.h> 		/* for toupper - should be in tcp.h */
#ifdef VMS
extern char * getpass PARAMS((CONST char *prompt));
#endif /* VMS */

#include "LYLeaks.h"

PUBLIC void HTAlert ARGS1(CONST char *, Msg)
{
#ifdef NeXTStep
    NXRunAlertPanel(NULL, "%s", NULL, NULL, NULL, Msg);
#else
    fprintf(stderr, "WWW Alert:  %s\n", Msg);
#endif
}


PUBLIC void HTProgress ARGS1(CONST char *, Msg)
{
    fprintf(stderr, "   %s ...\n", Msg);
}


PUBLIC BOOL HTConfirm ARGS1(CONST char *, Msg)
{
  char Reply[4];	/* One more for terminating NULL -- AL */
  char *URep;
  
  fprintf(stderr, "WWW: %s (y/n) ", Msg);
                       /* (y/n) came twice -- AL */

  fgets(Reply, 4, stdin); /* get reply, max 3 characters */
  URep=Reply;
  while (*URep) {
    if (*URep == '\n') {
	*URep = (char)0;	/* Overwrite newline */
	break;
    }
    *URep=TOUPPER(*URep);
    URep++;	/* This was previously embedded in the TOUPPER */
                /* call an it became evaluated twice because   */
                /* TOUPPER is a macro -- AL */
  }

  if ((strcmp(Reply,"YES")==0) || (strcmp(Reply,"Y")==0))
    return(YES);
  else
    return(NO);
}

/*	Prompt for answer and get text back
*/
PUBLIC char * HTPrompt ARGS2(CONST char *, Msg, CONST char *, deflt)
{
    char Tmp[200];
    char * rep = 0;
    fprintf(stderr, "WWW: %s", Msg);
    if (deflt) fprintf(stderr, " (RETURN for [%s]) ", deflt);
    
    fgets(Tmp, 200, stdin);
    Tmp[strlen(Tmp)-1] = (char)0;	/* Overwrite newline */
   
    StrAllocCopy(rep, *Tmp ? Tmp : deflt);
    return rep;
}


/*	Prompt for password without echoing the reply
*/
PUBLIC char * HTPromptPassword ARGS1(CONST char *, Msg)
{
    char *result = NULL;
    char *pw = (char*)getpass(Msg ? Msg : "Password: ");

    StrAllocCopy(result, pw);
    return result;
}


/*	Prompt both username and password	HTPromptUsernameAndPassword()
**	---------------------------------
** On entry,
**	Msg		is the prompting message.
**	*username and
**	*password	are char pointers; they are changed
**			to point to result strings.
**
**			If *username is not NULL, it is taken
**			to point to  a default value.
**			Initial value of *password is
**			completely discarded.
**
** On exit,
**	*username and *password point to newly allocated
**	strings -- original strings pointed to by them
**	are NOT freed.
**	
*/
PUBLIC void HTPromptUsernameAndPassword ARGS4(CONST char *,	Msg,
					      char **,		username,
					      char **,		password,
					      BOOL,		IsProxy)
{
    if (Msg)
	fprintf(stderr, "WWW: %s\n", Msg);
    *username = HTPrompt("Username: ", *username);
    *password = HTPromptPassword("Password: ");
}

