/*
 * run.c --- stubs for unused cvs functions in 'src/run.c'
 *
 * MDLadwig <mike@twinpeaks.prc.com> --- Nov 1995
 */

#include "mac_config.h"
#include <sys/types.h>
#include <stdio.h>

void run_arg (const char *s) { }
void run_print (FILE * fp) { }
void run_setup (const char *prog) { }
int run_exec (char *stin, char *stout, char *sterr, int flags) { return 0; }
FILE * run_popen(const char *, const char *) { return NULL; }
int pclose(FILE *fp) { return 0; }
int piped_child (char **, int *, int *) { return 0; }
void close_on_exec (int) { }
int filter_stream_through_program (int, int, char **, pid_t *) { return 0; }
