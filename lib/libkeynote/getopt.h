/* $OpenBSD: getopt.h,v 1.1 1999/10/01 01:08:30 angelos Exp $ */
extern int   opterr;   /* flag:error message on unrecognzed options */
extern int   optind;   /* last touched cmdline argument */
extern char  *optarg;  /* argument to optopt */
int getopt(int argc, char **argv, char *opts);
