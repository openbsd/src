/*                   /Net/dxcern/userd/timbl/hypertext/WWW/Library/Implementation/HTInit.html
                                  INITIALISATION MODULE
                                             
   This module resisters all the plug & play software modules which will be used in the
   program.  This is for a browser.
   
   To override this, just copy it and link in your version befoe you link with the
   library.
   
   Implemented by HTInit.c by default.
   
 */
#ifndef HTUTILS_H
#include "HTUtils.h"
#endif /* HTUTILS_H */

extern void HTFormatInit NOPARAMS;
extern void HTPreparsedFormatInit NOPARAMS;
extern void HTFileInit NOPARAMS;

/*

    */
