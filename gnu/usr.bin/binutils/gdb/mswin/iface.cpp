

#include "stdafx.h"
#include "regdoc.h"
#include "bptdoc.h"
#include "thinking.h"
#include "log.h"

extern "C" {
	void gdbwin_fputs(const char *linebuffer, FILE *stream);
	void gdbwin_update(int regs, int pc, int bpt);
	void gdbwin_command (const char *);
};

Credirect *redirect;

Credirect::Credirect(CString *p)
{
prev = redirect;
redirect = this;
ptr = p;
}

Credirect::Credirect()
{
prev = redirect;
redirect = this;
ptr = 0;
}
Credirect::~Credirect()
{
redirect = prev;
}

void gdbwin_fputs(const char *string, FILE *stream)
{
  extern CCmdLogView *cmdwinptr;

  if (redirect)	
    {
      redirect->add(string);
    }
  else if (cmdwinptr)
    {
      cmdwinptr->add((char *)string);
    }
}
void theApp_sync_watch();
extern CGuiApp theApp;
void theApp_sync_pc();
extern CBreakInfoList the_breakinfo_list;
void gdbwin_update(int regs, int pc, int bpt)
{
  if (regs || pc) {
    CGuiApp::SyncRegs();
    theApp_sync_watch();
  }

  if (bpt)
    {
      theApp.sync_bpts();
    }
  if (pc) {
    theApp_sync_pc();
  }

  if (bpt) 
    {
      the_breakinfo_list.Update();
      CBptDoc::AllSync();
    }

}

extern CGuiApp theApp;
void gdbwin_command(const char *command)
{
  theApp.Command(command);
}



/* Cope with calls to error inside gdb stuff.  
   When all goes well we can call gdb and it will return happily
   in the usual C way.

   When something goes wrong, the guts of gdb will call error()
   which likes to longjmp to the mainloop, where all the temp
   stuff which was bought gets thrown away.  And once in the loop
   the GUI looses control over gdb.

   So we invent a class, which when constructed remembers the
   cleanup stack and plays with the longjmp buffer.  If a function
   is exited normally we've nothing to do.  If it longjmps out
   to what it thought was the main loop, we come here.  We unwind
   all the temps which are new since the class was constructed, and
   return a fail to the caller.

   It would be nice to use c++ exception handling here, but
   gdb's cleanups sorta get in the way.

*/


extern "C" {
  jmp_buf error_return;
  extern struct cleanup *save_cleanups();
/*  extern void *error_hook;*/
}
jmp_buf gobuf;

void jumpit()
{
  longjmp (gobuf, 1);
}
CErrorWrap::CErrorWrap()
{
  prev_error_hook = error_hook;
  error_hook = jumpit;
  saved_cleanup_chain = save_cleanups ();
  firsttime = 1;
  passed = 1;

}


CErrorWrap::~CErrorWrap()
{
  togdb_do_cleanups_ALL_CLEANUPS();
  togdb_restore_cleanups (saved_cleanup_chain);
  error_hook = prev_error_hook;
}

int CErrorWrap::gdb_try()
{
  int x = firsttime;
  passed = 1;
  /* If setjmp returns nozero then we jumped here */
  if (setjmp (gobuf)!=0)
    passed = 0;
  firsttime = 1;
  return x;
}
int CErrorWrap::gdb_catch()
{
 return !passed; 
}

extern CStatusBar *status;

extern "C" 
{
  static int won = 0;
  const char * doing_something(const char *n) 
    {
      static const char *prev;
      const char *p =prev;
      prev = n;
      status->SetWindowText(n);
      status->SendMessage(WM_PAINT);
      if (n)
	{ 
	  if (!won)
	    { 
	      won= 1;
	      AfxGetApp()->BeginWaitCursor(); 
	    }
	}
      else
	{
	  if (won)
	    {
	      AfxGetApp()->EndWaitCursor(); 
	      won = 0; 
	    }
	}
      return p;
    }
}

#if 0
void string_command(const CString &what)
{
const char *p = (const char *)(what)();
togdb_command(p);
}


#endif
#undef  make_cleanup 

#if 0
extern "C" 
{
  extern struct cleanup *make_cleanup (void *, void *);
};
struct cleanup *my_make_cleanup  (void *fptr, void *r)
{



  return make_cleanup(fptr, r);
}
#endif
