/*

readpass.c

Author: Tatu Ylonen <ylo@cs.hut.fi>

Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
                   All rights reserved

Created: Mon Jul 10 22:08:59 1995 ylo

Functions for reading passphrases and passwords.

*/

#include "includes.h"
RCSID("$Id: readpass.c,v 1.1 1999/09/26 20:53:37 deraadt Exp $");

#include "xmalloc.h"
#include "ssh.h"

/* Saved old terminal mode for read_passphrase. */
#ifdef USING_TERMIOS
static struct termios saved_tio;
#endif
#ifdef USING_SGTTY
static struct sgttyb saved_tio;
#endif

/* Old interrupt signal handler for read_passphrase. */
static RETSIGTYPE (*old_handler)(int sig) = NULL;

/* Interrupt signal handler for read_passphrase. */

RETSIGTYPE intr_handler(int sig)
{
  /* Restore terminal modes. */
#ifdef USING_TERMIOS
  tcsetattr(fileno(stdin), TCSANOW, &saved_tio);
#endif
#ifdef USING_SGTTY
  ioctl(fileno(stdin), TIOCSETP, &saved_tio);
#endif
  /* Restore the old signal handler. */
  signal(sig, old_handler);
  /* Resend the signal, with the old handler. */
  kill(getpid(), sig);
}

/* Reads a passphrase from /dev/tty with echo turned off.  Returns the 
   passphrase (allocated with xmalloc).  Exits if EOF is encountered. 
   The passphrase if read from stdin if from_stdin is true (as is the
   case with ssh-keygen).  */

char *read_passphrase(const char *prompt, int from_stdin)
{
  char buf[1024], *cp;
#ifdef USING_TERMIOS
  struct termios tio;
#endif
#ifdef USING_SGTTY
  struct sgttyb tio;
#endif
  FILE *f;
  
  if (from_stdin)
    f = stdin;
  else
    {
      /* Read the passphrase from /dev/tty to make it possible to ask it even 
	 when stdin has been redirected. */
      f = fopen("/dev/tty", "r");
      if (!f)
	{
	  if (getenv("DISPLAY"))
	    {
	      char command[512];
	      fprintf(stderr,
		      "Executing ssh-askpass to query the password...\n");
	      fflush(stdout);
	      fflush(stderr);
	      sprintf(command, "ssh-askpass '%.400s'", prompt);
	      f = popen(command, "r");
	      if (!fgets(buf, sizeof(buf), f))
		{
		  pclose(f);
		  fprintf(stderr, "No passphrase supplied.  Exiting.\n");
		  exit(1);
		}
	      pclose(f);
	      if (strchr(buf, '\n'))
		*strchr(buf, '\n') = 0;
	      return xstrdup(buf);
	    }

	  /* No controlling terminal and no DISPLAY.  Nowhere to read. */
	  fprintf(stderr, "You have no controlling tty and no DISPLAY.  Cannot read passphrase.\n");
	  exit(1);
	}
    }

  /* Display the prompt (on stderr because stdout might be redirected). */
  fflush(stdout);
  fprintf(stderr, "%s", prompt);
  fflush(stderr);

  /* Get terminal modes. */
#ifdef USING_TERMIOS
  tcgetattr(fileno(f), &tio);
#endif
#ifdef USING_SGTTY
  ioctl(fileno(f), TIOCGETP, &tio);
#endif
  saved_tio = tio;
  /* Save signal handler and set the new handler. */
  old_handler = signal(SIGINT, intr_handler);

  /* Set new terminal modes disabling all echo. */
#ifdef USING_TERMIOS
  tio.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
  tcsetattr(fileno(f), TCSANOW, &tio);
#endif
#ifdef USING_SGTTY
  tio.sg_flags &= ~(ECHO);
  ioctl(fileno(f), TIOCSETP, &tio);
#endif

  /* Read the passphrase from the terminal. */
  if (fgets(buf, sizeof(buf), f) == NULL)
    {
      /* Got EOF.  Just exit. */
      /* Restore terminal modes. */
#ifdef USING_TERMIOS
      tcsetattr(fileno(f), TCSANOW, &saved_tio);
#endif
#ifdef USING_SGTTY
      ioctl(fileno(f), TIOCSETP, &saved_tio);
#endif
      /* Restore the signal handler. */
      signal(SIGINT, old_handler);
      /* Print a newline (the prompt probably didn\'t have one). */
      fprintf(stderr, "\n");
      /* Close the file. */
      if (f != stdin)
	fclose(f);
      exit(1);
    }
  /* Restore terminal modes. */
#ifdef USING_TERMIOS
  tcsetattr(fileno(f), TCSANOW, &saved_tio);
#endif
#ifdef USING_SGTTY
  ioctl(fileno(f), TIOCSETP, &saved_tio);
#endif
  /* Restore the signal handler. */
  (void)signal(SIGINT, old_handler);
  /* Remove newline from the passphrase. */
  if (strchr(buf, '\n'))
    *strchr(buf, '\n') = 0;
  /* Allocate a copy of the passphrase. */
  cp = xstrdup(buf);
  /* Clear the buffer so we don\'t leave copies of the passphrase laying
     around. */
  memset(buf, 0, sizeof(buf));
  /* Print a newline since the prompt probably didn\'t have one. */
  fprintf(stderr, "\n");
  /* Close the file. */
  if (f != stdin)
    fclose(f);
  return cp;
}
