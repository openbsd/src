/* $OpenBSD: getopt.c,v 1.1 1999/10/01 01:08:29 angelos Exp $ */
#if HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <ctype.h>

#if STDC_HEADERS
#include <string.h>
#if !defined(HAVE_STRCHR)
#define strchr index
#endif /* !HAVE_STRCHR */
#endif /* STDC_HEADERS */

/*** getopt
 *
 *   This function is the public domain version of getopt, the command
 *   line argument processor, and hence is NOT copyrighted by Microsoft,
 *   IBM, or AT&T.
 */

#define ERR(s, c)   if(opterr){\
   (void) fputs(argv[0], stderr);\
   (void) fputs(s, stderr);\
   (void) fputc(c, stderr);\
   (void) fputc('\n', stderr);}

int   opterr = 1;   /* flag:error message on unrecognzed options */
int   optind = 1;   /* last touched cmdline argument */
int   optopt;       /* last returned option */
char  *optarg;      /* argument to optopt */
int   getopt(int argc, char **argv, char *opts);

/* int    argc is the number of arguments on cmdline */
/* char   **argv is the pointer to array of cmdline arguments */
/* char   *opts is the string of all valid options   */
/* each char case must be given; options taking an arg are followed by =
':' */
int getopt(int argc, char **argv, char *opts)
{
   static int sp = 1;
   register int c;
   register char *cp;
   if(sp == 1)
      /* check for end of options */
      if(optind >= argc ||
          (argv[optind][0] != '/' &&
          argv[optind][0] != '-') ||
          argv[optind][1] == '\0')
         return(EOF);
      else if(!strcmp(argv[optind], "--")) {
         optind++;
         return(EOF);
      }
   optopt = c = argv[optind][sp];
   if(c == ':' || (cp=strchr(opts, c)) == NULL) {
      /* if arg sentinel as option or other invalid option,
         handle the error and return '?' */
      ERR(": illegal option -- ", (char)c);
      if(argv[optind][++sp] == '\0') {
         optind++;
         sp = 1;
      }
      return('?');
   }
   if(*++cp == ':') {
      /* if option is given an argument...  */
      if(argv[optind][sp+1] != '\0')
         /* and the OptArg is in that CmdLineArg, return it... */
         optarg = &argv[optind++][sp+1];
      else if(++optind >= argc) {
         /* but if the OptArg isn't there and the next CmdLineArg
            isn't either, handle the error... */
         ERR(": option requires an argument -- ", (char)c);
         sp = 1;
         return('?');
      } else
         /* but if there is another CmdLineArg there, return that */
         optarg = argv[optind++];
      /* and set up for the next CmdLineArg */
      sp = 1;
   } else {
      /* no arg for this opt, so null arg and set up for next option */
      if(argv[optind][++sp] == '\0') {
         sp = 1;
         optind++;
      }
      optarg = NULL;
   }
   return(c);
}
