/* alloca.h --- Windows NT version of alloca.h
   Jim Blandy <jimb@cyclic.com> --- July 1995  */

/* Here's the situation.

   CVS uses alloca.  Not in many places, but that number is likely to grow
   since the GNU coding standards give alloca official blessing, and
   supposedly autoconf takes care of the portability problems.

   Windows NT has alloca, but calls it _alloca and declares it to return
   void *.

   The autoconf manual provides a big wad of CPP cruft to place in files
   that want to use alloca; it currently appears in lib/system.h,
   lib/regex.c, and in src/cvs.h.  This boilerplate wad says that if you
   HAVE_ALLOCA but don't HAVE_ALLOCA_H, you should declare alloca as an
   extern function returning char *.

   This may be fine for most systems, but it makes Visual C++ barf,
   because the return types conflict.  So the workaround is to
   actually have an alloca.h file that declares things appropriately.
   The boilerplate alloca wad says that if you HAVE_ALLOCA_H, let it
   declare everything for you.  Which suits us fine.  */

#define alloca _alloca
extern void *alloca ();
