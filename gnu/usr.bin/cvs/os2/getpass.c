#include <stdio.h>
#include <string.h>
#include "cvs.h"
#include "os2inc.h"

/* Only define this if you're testing and want to compile this file
   standalone. */
/* #define DIAGNOSTIC */

/* Turn off keyboard echo.  Does not check error returns. */
static void
EchoOff (void)
{
  KBDINFO KbdInfo;
  
  KbdGetStatus (&KbdInfo, 0);
  KbdInfo.fsMask = (KbdInfo.fsMask & ~KEYBOARD_ECHO_ON) | KEYBOARD_ECHO_OFF;
  KbdSetStatus (&KbdInfo, 0);
}

/* Turn on keyboard echo.  Does not check error returns. */
static void
EchoOn( void )
{
  KBDINFO KbdInfo;
  
  KbdGetStatus (&KbdInfo, 0);
  KbdInfo.fsMask = (KbdInfo.fsMask & ~KEYBOARD_ECHO_OFF) | KEYBOARD_ECHO_ON;
  KbdSetStatus (&KbdInfo, 0);
}

char *
getpass (char *prompt)
{
  static char Buf[80];
  STRINGINBUF StringInBuf;
  
  printf ("%s", prompt);
  fflush (stdout);

  EchoOff ();

  StringInBuf.cb = sizeof (Buf) - 1;
  StringInBuf.cchIn = 0;
  KbdStringIn ((PSZ) Buf, &StringInBuf, IO_WAIT, 0);
  Buf[StringInBuf.cchIn] = '\0';

  EchoOn ();

  return Buf;
}


#ifdef DIAGNOSTIC
main()
{
  char *s;
  s = getpass ("Input password (no echo): ");
  printf ("String was \"%s\"\n", s);
  fflush (stdout);
}
#endif /* DIAGNOSTIC */

