#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

void foo (void);
void bar (void);

void subroutine (int);
void handler (int);
void have_a_very_merry_interrupt (void);

main ()
{
  puts ("Starting up");

  foo ();   /* Put a breakpoint on foo() and call it to see a dummy frame */


  have_a_very_merry_interrupt ();

  puts ("Shutting down");
}

void
foo (void)
{
  puts ("hi in foo");
}

void 
bar (void)
{
  char *nuller = 0;

  puts ("hi in bar");

  *nuller = 'a';      /* try to cause a segfault */
}

void
handler (int sig)
{
  subroutine (sig);
}

/* The first statement in subroutine () is a place for a breakpoint.  
   Without it, the breakpoint is put on the while comparison and will
   be hit at each iteration. */

void
subroutine (int in)
{
  int count = in;
  while (count < 100)
    count++;
}

void
have_a_very_merry_interrupt (void)
{
  puts ("Waiting to get a signal");
  signal (SIGALRM, handler);
  alarm (1);
  sleep (2);  /* We'll receive that signal while sleeping */
}

