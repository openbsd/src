/* popen.c -- popen/pclose for OS/2. */

/* Set to 0 for distribution.
   Search for "DIAGNOSTIC" in the code to see what it's for. */
#define DIAGNOSTIC 0

#include    <process.h>

#include    <stdio.h>
#include    <stdlib.h>
#include    <sys/types.h>
#include    <sys/stat.h>
#include    <ctype.h>
#include    <string.h>
#include    <fcntl.h>

#include    "config.h"
#include    "os2inc.h"

#define LL_VAL ULONG
#define LL_KEY PID    /* also ULONG, really */

#define STDIN       0
#define STDOUT      1
#define STDERR      2

/* ********************************************************************* *
 *                                                                       *
 *   First, a little linked-list library to help keep track of pipes:    *
 *                                                                       *
 * ********************************************************************* */

/* Map integer PID's onto integer termination codes. */
struct pid_list
{
  HFILE pid;              /* key */
  ULONG term_code;        /* val */
  struct pid_list *next;  /* duh */ 
};

static struct pid_list *pid_ll = (struct pid_list *) NULL;

/* The ll_*() functions all make use of the global var `pid_ll'. */

void
ll_insert (HFILE key, ULONG val)
{
  struct pid_list *new;
  new = (struct pid_list *) malloc (sizeof (*new));

  new->pid       = key;
  new->term_code = val;
  new->next      = pid_ll;

  pid_ll = new;
}


void
ll_delete (int key)
{
  struct pid_list *this, *last;

  this = pid_ll;
  last = (struct pid_list *) NULL;

  while (this)
    {
      if (this->pid == key)
        {
          /* Delete this node and leave. */
          if (last)
            last->next = this->next;
          else
            pid_ll = this->next;
          free (this);
          return;
        }

      /* Else no match, so try the next one. */
      last = this;
      this = this->next;
    }
}

ULONG
ll_lookup (HFILE key)
{
  struct pid_list *this = pid_ll;

  while (this)
    {
      if (this->pid == key)
        return this->term_code;

      /* Else no match, so try the next one. */
      this = this->next;
    }

  /* Zero is special in this context anyway. */
  return 0;
}

#if DIAGNOSTIC
ULONG
ll_length ()
{
  struct pid_list *this = pid_ll;
  unsigned long int len;

  for (len = 0; this; len++)
    this = this->next;

  return len;
}

ULONG
ll_print ()
{
  struct pid_list *this = pid_ll;
  unsigned long int i;

  for (i = 0; this; i++)
    {
      printf ("pid_ll[%d] == (%5d --> %5d)\n",
			  i, this->pid, this->term_code);
      this = this->next;
    }

  if (i == 0)
    printf ("No entries.\n");

  return i;
}
#endif /* DIAGNOSTIC */

/* ********************************************************************* *
 *                                                                       *
 *       End of linked-list library, beginning of popen/pclose:          *
 *                                                                       *
 * ********************************************************************* */

/*
 *  Routine: popen
 *  Returns: FILE pointer to pipe.
 *  Action : Exec program connected via pipe, connect a FILE * to the
 *           pipe and return it.
 *  Params : Command - Program to run
 *           Mode    - Mode to open pipe.  "r" implies pipe is connected
 *                     to the programs stdout, "w" connects to stdin.
 */
FILE *
popen (const char *Command, const char *Mode)
{
    HFILE End1, End2, Std, Old1, Old2, Tmp;

    FILE *File;

    char    Fail[256],
            *Args,
            CmdLine[256],
            *CmdExe;

    RESULTCODES
            Result;

    int     Rc;

    if (DosCreatePipe (&End1, &End2, 4096))
        return NULL;

    Std = (*Mode == 'w') ? STDIN : STDOUT ;
    if (Std == STDIN)
    {
        Tmp = End1; End1 = End2; End2 = Tmp;
    }

    Old1 = -1; /* save stdin or stdout */
    DosDupHandle (Std, &Old1);
    DosSetFHState (Old1, OPEN_FLAGS_NOINHERIT);
    Tmp = Std; /* redirect stdin or stdout */
    DosDupHandle (End2, &Tmp);

    if (Std == 1) 
    {
        Old2 = -1; /* save stderr */
        DosDupHandle (STDERR, &Old2);
        DosSetFHState (Old2, OPEN_FLAGS_NOINHERIT);
        Tmp = STDERR;
        DosDupHandle (End2, &Tmp);
    }

    DosClose (End2);
    DosSetFHState (End1, OPEN_FLAGS_NOINHERIT);

    if ((CmdExe = getenv ("COMSPEC")) == NULL )
        CmdExe = "cmd.exe";

    strcpy (CmdLine, CmdExe);
    Args = CmdLine + strlen (CmdLine) + 1; /* skip zero */
    strcpy (Args, "/c ");
    strcat (Args, Command);
    Args[strlen (Args) + 1] = '\0'; /* two zeroes */
    Rc = DosExecPgm (Fail, sizeof (Fail), EXEC_ASYNCRESULT, 
                     (unsigned char *) CmdLine, 0, &Result,
                     (unsigned char *) CmdExe);

    Tmp = Std; /* restore stdin or stdout */
    DosDupHandle (Old1, &Tmp);
    DosClose (Old1);

    if (Std == STDOUT) 
    {
        Tmp = STDERR;   /* restore stderr */
        DosDupHandle (Old2, &Tmp);
        DosClose (Old2);
    }

    if (Rc)
    {
        DosClose (End1);
        return NULL;
    }
  
#ifdef __WATCOMC__
    /* Watcom does not allow mixing operating system handles and
     * C library handles, so we have to convert.
     */
    File = fdopen (_hdopen (End1, *Mode == 'w'? O_WRONLY : O_RDONLY), Mode);
#else
    File = fdopen (End1, Mode);
#endif
    ll_insert ((LL_KEY) End1, (LL_VAL) Result.codeTerminate);

    return File;
}
        

/*
 *  Routine: popenRW
 *  Returns: PID of child process
 *  Action : Exec program connected via pipe, connect int fd's to 
 *           both the stdin and stdout of the process.
 *  Params : Command - Program to run
 *           Pipes   - Array of 2 ints to store the pipe descriptors
 *                     Pipe[0] writes to child's stdin,
 *                     Pipe[1] reads from child's stdout/stderr
 */
int
popenRW (const char **argv, int *pipes)
{
    HFILE Out1, Out2, In1, In2;
    HFILE Old0 = -1, Old1 = -1, Old2 = -1, Tmp;

    int pid;

    if (DosCreatePipe (&Out2, &Out1, 4096))
        return FALSE;

    if (DosCreatePipe (&In1, &In2, 4096))
    {
        DosClose (Out1);
        DosClose (Out2);
        return FALSE;
    }

    /* Save std{in,out,err} */
    DosDupHandle (STDIN, &Old0);
    DosSetFHState (Old1, OPEN_FLAGS_NOINHERIT);
    DosDupHandle (STDOUT, &Old1);
    DosSetFHState (Old2, OPEN_FLAGS_NOINHERIT);
    DosDupHandle (STDERR, &Old2);
    DosSetFHState (Old2, OPEN_FLAGS_NOINHERIT);

    /* Redirect std{in,out,err} */
    Tmp = STDIN;
    DosDupHandle (In1, &Tmp);
    Tmp = STDOUT;
    DosDupHandle (Out1, &Tmp);
    Tmp = STDERR;
    DosDupHandle (Out1, &Tmp);

    /* Close file handles not needed in child */
    
    DosClose (In1);
    DosClose (Out1);
    DosSetFHState (In2, OPEN_FLAGS_NOINHERIT);
    DosSetFHState (Out2, OPEN_FLAGS_NOINHERIT);

    /* Spawn we now our hoary brood. */
    pid = spawnvp (P_NOWAIT, argv[0], argv);

    /* Restore std{in,out,err} */
    Tmp = STDIN;
    DosDupHandle (Old0, &Tmp);
    DosClose (Old0);
    Tmp = STDOUT;
    DosDupHandle (Old1, &Tmp);
    DosClose (Old1);
    Tmp = STDERR;
    DosDupHandle (Old2, &Tmp);
    DosClose (Old2);

    if(pid < 0) 
      {
        DosClose (In2);
        DosClose (Out2);
        return -1;
      }
    
    pipes[0] = In2;
    _setmode (pipes[0], O_BINARY);
    pipes[1] = Out2;
    _setmode (pipes[1], O_BINARY);

    /* Save ID of write-to-child pipe for pclose() */
    ll_insert ((LL_KEY) In2, (LL_VAL) pid);
    
    return pid;
}


/*
 *  Routine: pclose
 *  Returns: TRUE on success
 *  Action : Close a pipe opened with popen();
 *  Params : Pipe - pipe to close
 */
int
pclose (FILE *Pipe) 
{
    RESULTCODES rc;
    PID pid, pid1;
    int Handle = fileno (Pipe);

    fclose (Pipe);

    rc.codeTerminate = -1;

    pid1 = (PID) ll_lookup ((LL_KEY) Handle);
    /* if pid1 is zero, something's seriously wrong */
    if (pid1 != 0)
      {
        DosWaitChild (DCWA_PROCESSTREE, DCWW_WAIT, &rc, &pid, pid1);
        ll_delete ((LL_KEY) Handle);
      }
    return rc.codeTerminate == 0 ? rc.codeResult : -1;
}


#if DIAGNOSTIC
void
main ()
{
  FILE *fp1, *fp2, *fp3;
  int c;

  ll_print ();
  fp1 = popen ("gcc --version", "r");
  ll_print ();
  fp2 = popen ("link386 /?", "r");
  ll_print ();
  fp3 = popen ("dir", "r");
  ll_print ();

  while ((c = getc (fp1)) != EOF)
    printf ("%c", c);

  while ((c = getc (fp2)) != EOF)
    printf ("%c", c);

  while ((c = getc (fp3)) != EOF)
    printf ("%c", c);

  pclose (fp1);
  ll_print ();
  pclose (fp2);
  ll_print ();
  pclose (fp3);
  ll_print ();

  return;
}

#endif /* DIAGNOSTIC */
