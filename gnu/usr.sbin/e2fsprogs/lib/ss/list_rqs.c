/*
 * Copyright 1987, 1988 by MIT Student Information Processing Board
 *
 * For copyright information, see copyright.h.
 */
#include "copyright.h"
#include "ss_internal.h"
#include <signal.h>
#include <setjmp.h>
#include <sys/wait.h>

typedef void sigret_t;

#ifdef lint     /* "lint returns a value which is sometimes ignored" */
#define DONT_USE(x)     x=x;
#else /* !lint */
#define DONT_USE(x)     ;
#endif /* lint */

static char const twentyfive_spaces[26] =
    "                         ";
static char const NL[2] = "\n";

void ss_list_requests(argc, argv, sci_idx, info_ptr)
    int argc;
    char **argv;
    int sci_idx;
    pointer info_ptr;
{
    register ss_request_entry *entry;
    register char const * const *name;
    register int spacing;
    register ss_request_table **table;

    char buffer[BUFSIZ];
    FILE *output;
    int fd;
#ifdef POSIX_SIGNALS
    sigset_t omask, igmask;
#else
    int mask;
#endif
    sigret_t (*func)();
#ifndef NO_FORK
    int waitb;
#endif

    DONT_USE(argc);
    DONT_USE(argv);

#ifdef POSIX_SIGNALS
    sigemptyset(&igmask);
    sigaddset(&igmask, SIGINT);
    sigprocmask(SIG_BLOCK, &igmask, &omask);
#else
    mask = sigblock(sigmask(SIGINT));
#endif
    func = signal(SIGINT, SIG_IGN);
    fd = ss_pager_create();
    output = fdopen(fd, "w");
#ifdef POSIX_SIGNALS
    sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#else
    sigsetmask(mask);
#endif

    fprintf (output, "Available %s requests:\n\n",
	     ss_info (sci_idx) -> subsystem_name);

    for (table = ss_info(sci_idx)->rqt_tables; *table; table++) {
        entry = (*table)->requests;
        for (; entry->command_names; entry++) {
            spacing = -2;
            buffer[0] = '\0';
            if (entry->flags & SS_OPT_DONT_LIST)
                continue;
            for (name = entry->command_names; *name; name++) {
                register int len = strlen(*name);
                strncat(buffer, *name, len);
                spacing += len + 2;
                if (name[1]) {
                    strcat(buffer, ", ");
                }
            }
            if (spacing > 23) {
                strcat(buffer, NL);
                fputs(buffer, output);
                spacing = 0;
                buffer[0] = '\0';
            }
            strncat(buffer, twentyfive_spaces, 25-spacing);
            strcat(buffer, entry->info_string);
            strcat(buffer, NL);
            fputs(buffer, output);
        }
    }
    fclose(output);
#ifndef NO_FORK
    wait(&waitb);
#endif
    (void) signal(SIGINT, func);
}
