
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	nwplglob.c
 * DESCRIPTION	:	Perl globbing support for NetWare. Other platforms have usually launched
 *                  a separate executable for this in order to take advantage of their
 *                  shell's capability for generating a list of files from a given
 *                  wildcard file spec. On NetWare, we don't have that luxury.
 *                  So we just hack the support into pipe open support (which we also had to hack).
 * Author		:	HYAK
 * Date			:	January 2001.
 *
 */



#include <nwtypes.h>
#include "stdio.h"
#include <dirent.h>

#include "win32ish.h"
#include "nwplglob.h"



/*============================================================================================

 Function		:	fnDoPerlGlob

 Description	:	Perl globbing support: Takes an array of wildcard descriptors
                    and produces from it a list of files that the wildcards expand into.
					The list of files is written to the temporary file named by fileName.

 Parameters 	:	argv (IN)	-	Input argument vector.
                    fileName (IN)	-	Input file name for storing globed file names.

 Returns		:	Nothing.

==============================================================================================*/

void fnDoPerlGlob(char** argv, char* fileName)
{
	FILE * redirOut = NULL;

	if (*argv)
		argv++;
	if (*argv == NULL)
		return;

	redirOut = fopen((const char *)fileName, (const char *)"w");
	if (!redirOut)
		return;

	do
	{
		DIR* dir = NULL;
		DIR* fil = NULL;
		char* pattern = NULL;

		pattern = *argv++;

		dir = opendir((const char *)pattern);
		if (!dir)
			continue;

		/* find the last separator in pattern, NetWare has three: /\: */
		while (fil = readdir(dir))
		{
			// The below displays the files separated by tab character.
			// Also, it displays only the file names and not directories.
			// If any other format is desired, it needs to be done here.
			fprintf(redirOut, "%s\t", fil->d_name);
		}

		closedir(dir);

	} while (*argv);

	fclose(redirOut);

	return;
}

