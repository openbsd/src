/*
 * Globbing for NT.  Relies on the expansion done by the library
 * startup code. 
 */

#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <string.h>
#include <windows.h>

int
main(int argc, char *argv[])
{
    int i;
    int len;
    char root[MAX_PATH];
    char *dummy;
    char volname[MAX_PATH];
    DWORD serial, maxname, flags;
    BOOL downcase = TRUE;

    /* check out the file system characteristics */
    if (GetFullPathName(".", MAX_PATH, root, &dummy)) {
        dummy = strchr(root,'\\'); 
	if (dummy)
	    *++dummy = '\0';
	if (GetVolumeInformation(root, volname, MAX_PATH, 
				 &serial, &maxname, &flags, 0, 0)) {
	    downcase = !(flags & FS_CASE_IS_PRESERVED);
	}
    }

    setmode(fileno(stdout), O_BINARY);
    for (i = 1; i < argc; i++) {
	len = strlen(argv[i]);
	if (downcase)
	    strlwr(argv[i]);
	if (i > 1) fwrite("\0", sizeof(char), 1, stdout);
	fwrite(argv[i], sizeof(char), len, stdout);
    }
    return 0;
}

