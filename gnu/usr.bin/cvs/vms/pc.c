#include <stdio.h>
#include <unixio.h>
#include <stdlib.h>
#include <string.h>

int trace = 1;

extern int piped_child();
extern int piped_child_shutdown();

main (argc, argv)
  int argc;
  char ** argv;
{
  static char line[512];
  char *linep[] = {line, NULL};
  int pid;
  int tofd, fromfd;
  FILE *in, *out;
  
  while (1)
    {
    printf("\nEnter a command to run: ");
    line[0] = '\0';
    fgets(line, 511, stdin);
    if (!strlen(line))
       exit(2);

    line[strlen(line)-1] = '\0';
    pid = piped_child(linep, &tofd, &fromfd);

    in = fdopen(fromfd, "r");
    out = fdopen(tofd, "w");

#if 0
    out = fdopen(tofd, "w");
    fclose(out);
#endif

    do
       {
       if(!feof(stdin))
         {
         fprintf(stdout, "> ");
         fgets(line, 511, stdin);
         fputs(line, out);
         }
       else 
         {
         fclose(out);
         close(tofd);
         }

       fgets(line, 511, in);
       fputs(line, stdout);
       line[0] = '\0';
       } while (!feof(in));

    fprintf(stderr, "waiting for child to stop\n");
    piped_child_shutdown(pid);
    }
}
