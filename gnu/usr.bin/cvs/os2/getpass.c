#include <stdio.h>
#include <string.h>
#include "cvs.h"

char *
getpass (char *prompt)
{
    static char passbuf[30];
    int i;
    char *p;
    int ch;
    
    printf ("%s", prompt);
    fflush (stdout);
    
    p = passbuf, i = 0;
    while (((ch = getchar ()) != '\n') && (ch != EOF))
    {
        if (i++ < 24)
            *p++ = ch;
    }
    *p = '\0';

    return passbuf;
}
