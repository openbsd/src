#include <stdio.h>
#include <curses.h>

char *
getpass (char *prompt)
{
    /* FIXME: arbitrary limit; I think we need to ditch getstr to fix it.  */
    static char buf[2048];

    /* This clears the screen, which is *not* what we want.  But until I
       get some real VMS documentation....  */
    initscr ();

    printw ("%s", prompt);
    refresh ();
    noecho ();
    getstr (buf);
    endwin ();
    printf ("\n");
    return buf;
}

#if 0
int
main ()
{
    printf ("thank you for saying \"%s\"\n", getpass ("What'll it be? "));
    return 0;
}
#endif
