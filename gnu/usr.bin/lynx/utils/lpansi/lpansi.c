/********************************************************
 * lpansi.c                                             *
 * Original author: Gary Day, 11/30/93                  *
 * Current version: 2.1 by Noel Hunter, 10/20/94        *
 *                                                      *
 * Basic structure based on print -- format files for   *
 * printing from _Pratical_C_Programming by Steve       *
 * Oualline, O'Reilly & Associates                      *
 ********************************************************/
#include <stdio.h>
#include <stdlib.h>      /* ANSI Standard only */
#include <string.h>
#ifdef VMS
#include <unixio.h>
#include <unixlib.h>
#endif /* VMS */

int verbose = 0;         /* verbose mode (default = false) */
int formfeed = 1;	 /* form feed mode (default = true) */
char *program_name;      /* name of the program (for errors) */

/* Ansi C function prototypes */
void ansi_printer_on(void);
void ansi_printer_off(void);
int main(int argc, char *argv[]);

main(int argc, char *argv[])
{
    void do_file(char *); /* print a file */
    void usage(void);     /* tell user how to use the program */

    /* save the program name for future use */
    program_name = argv[0];

    /* 
     * loop for each option.  
     *   Stop if we run out of arguments
     *   or we get an argument without a dash.
     */
    while ((argc > 1) && (argv[1][0] == '-')) {
        /*
         * argv[1][1] is the actual option character.
         */
        switch (argv[1][1]) {
            /*
             * -v verbose 
             */
            case 'v':
                verbose = 1; 
                (void)printf("VERBOSE mode ON\n");
                break;
            case 'f':
                formfeed = 0;
                break;
            default:
                (void)fprintf(stderr,"Bad option %s\n", argv[1]);
                usage();
        }
        /*
         * move the argument list up one
         * move the count down one
         */
        argv++;
        argc--;
    }

    /*
     * At this point all the options have been processed.
     * Check to see if we have no files in the list
     * and if so, we need to process just standard in.
     */
    if (argc == 1) {
        do_file("stdin");
    } else {
        while (argc > 1) {
          do_file(argv[1]);
          argv++;
          argc--;
        }
    }
    return (0);
}
/********************************************************
 * do_file -- send a file to the printer                *
 *                                                      *
 * Parameter                                            *
 *      name -- name of the file to print               *
 ********************************************************/
void do_file(char *name)
{
  int  ch;    /* Where we store our characters */
  FILE *fp;   /* File pointer */

  if ( verbose == 1 ) (void)printf("Processing: %s\n", name);

  ansi_printer_on();

  if ( strcmp(name,"stdin") == 0 )
  {
    while ((ch=getc(stdin))!=EOF)
    {
     putc(ch,stdout);
    }
  }
  else        /* Reading from a file */
  {
    if ((fp=fopen(name, "r"))==NULL)
    {
    printf("Can't open %s\n",name);
    exit(1);
    }
    while ((ch=getc(fp))!=EOF)
    {
     putc(ch,stdout);
    }
    fclose(fp);
  }

  if ( formfeed == 1 )
  {
    printf("\n\x0C");  /* Do a form feed at the end of the document */
  }

  ansi_printer_off();
 
  if ( verbose == 1 ) (void)printf("Finished processing: %s\n", name);

}

/********************************************************
 * usage -- tell the user how to use this program and   *
 *              exit                                    *
 ********************************************************/
void usage(void)
{
    (void)fprintf(stderr,"Usage is %s [options] [file-list]\n", 
                                program_name);
    (void)fprintf(stderr,"Options\n");
    (void)fprintf(stderr,"  -f          form feed OFF\n");
    (void)fprintf(stderr,"  -v          verbose\n");
    exit (1);
}

/********************************************************
 * Send a printer on escape sequence                    *
 *******************************************************/
void ansi_printer_on(void)
{
  if ( verbose == 1 ) (void)printf("ANSI printer ON\n");
  printf("\x1B[5i");
}

/********************************************************
 * Send a printer off escape sequence                   *
 *******************************************************/
void ansi_printer_off(void)
{
  printf("\x1B[4i");
  if ( verbose == 1 ) (void)printf("ANSI printer OFF\n");
}
