/* We roll our own popen()/pclose() in OS/2.
   Thanks, Glenn Gribble! */

FILE *popen (char *cmd, char *mode);
int popenRW (char **cmd, int *pipes);
int pclose (FILE *stream);
