/*
 External application support.
 This feature allows lynx to pass a given URL to an external program.
 It was written for three reasons.
 1) To overcome the deficiency	of Lynx_386 not supporting ftp and news.
    External programs can be used instead by passing the URL.

 2) To allow for background transfers in multitasking systems.
    I use wget for http and ftp transfers via the external command.

 3) To allow for new URLs to be used through lynx.
    URLs can be made up such as mymail: to spawn desired applications
    via the external command.

 See lynx.cfg for other info.
*/

#include "tcp.h"
#include "LYGlobalDefs.h"
#include "LYUtils.h"
#include "LYExtern.h"
#include "LYCurses.h"

#include "LYLeaks.h"

#ifdef USE_EXTERNALS
#define FREE(x) if (x) {free(x); x = NULL;}

void run_external ARGS1(char *, c)
{
	char command[1024];
	lynx_html_item_type *externals2=0;

	if (externals == NULL) return;

	for(externals2=externals; externals2 != NULL;
		 externals2=externals2->next)
	{

#ifdef _WINDOWS
	 if (!strnicmp(externals2->name,c,strlen(externals2->name)))
#else
	 if (!strncasecomp(externals2->name,c,strlen(externals2->name)))
#endif
	 {
	     char *cp;

		if(no_externals && !externals2->always_enabled)
		{
		  statusline(EXTERNALS_DISABLED);
		  sleep(MessageSecs);
		  return;
		}

		/*  Too dangerous to leave any URL that may come along unquoted.
		 *  They often contain '&', ';', and '?' chars, and who knows
		 *  what else may occur.
		 *  Prevent spoofing of the shell.
		 *  Dunno how this needs to be modified for VMS or DOS. - kw
		 */
#if defined(VMS) || defined(DOSPATH)
		sprintf(command, externals2->command, c);
#else /* Unix or DOS/Win: */
		cp = quote_pathname(c);
		sprintf(command, externals2->command, cp);
		FREE(cp);
#endif /* VMS */

		if (*command != '\0')
		{

		 statusline(command);
		 sleep(MessageSecs);

		 stop_curses();
		 fflush(stdout);
#ifdef __DJGPP__
		__djgpp_set_ctrl_c(0);
		_go32_want_ctrl_break(1);
#endif /* __DJGPP__ */
		 system(command);
#ifdef __DJGPP__
		__djgpp_set_ctrl_c(1);
		_go32_want_ctrl_break(0);
#endif /* __DJGPP__ */

		 fflush(stdout);
		 start_curses();
		}

		return;
	 }
	}

	return;
}
#endif /* USE_EXTERNALS */
