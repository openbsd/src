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

#include <LYUtils.h>

#ifdef USE_EXTERNALS

#include <HTAlert.h>
#include <LYGlobalDefs.h>
#include <LYExtern.h>
#include <LYLeaks.h>
#include <LYCurses.h>

void run_external ARGS1(char *, cmd)
{
    char *the_command = 0;
    lynx_html_item_type *ext = 0;

    for (ext = externals; ext != NULL; ext = ext->next) {

	if (ext->command != 0
	&& !strncasecomp(ext->name, cmd, strlen(ext->name))) {

	    if (no_externals && !ext->always_enabled) {
		HTUserMsg(EXTERNALS_DISABLED);
	    } else {

		HTAddParam(&the_command, ext->command, 1, cmd);
		HTEndParam(&the_command, ext->command, 1);

		HTUserMsg(the_command);

		stop_curses();
		LYSystem(the_command);
		FREE(the_command);
		start_curses();
	    }

	    break;
	}
    }

    return;
}
#endif /* USE_EXTERNALS */
