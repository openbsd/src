/* $Id: readers.c,v 1.3 2001/06/12 19:35:25 rees Exp $ */

/*
copyright 2001
the regents of the university of michigan
all rights reserved

permission is granted to use, copy, create derivative works 
and redistribute this software and such derivative works 
for any purpose, so long as the name of the university of 
michigan is not used in any advertising or publicity 
pertaining to the use or distribution of this software 
without specific, written prior authorization.  if the 
above copyright notice or any other identification of the 
university of michigan is included in any copy of any 
portion of this software, then the disclaimer below must 
also be included.

this software is provided as is, without representation 
from the university of michigan as to its fitness for any 
purpose, and without warranty by the university of 
michigan of any kind, either express or implied, including 
without limitation the implied warranties of 
merchantability and fitness for a particular purpose. the 
regents of the university of michigan shall not be liable 
for any damages, including special, indirect, incidental, or 
consequential damages, with respect to any claim arising 
out of or in connection with the use of the software, even 
if it has been or is hereafter advised of the possibility of 
such damages.
*/

/*
 * parse the reader.conf file
 *
 * Jim Rees
 * University of Michigan CITI, August 2000
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int
DBUpdateReaders(char *readerconf, int (callback) (int rn, unsigned long channelId, char *driverFile))
{
	FILE *f;
	char buf[512], lv[64], rv[512], libpath[512];
	long channelno;
	int nr = 0;

	f = fopen(readerconf, "r");
	if (!f)
		return -1;

	libpath[0] = '\0';
	channelno = -1;

	while (fgets(buf, sizeof buf, f)) {
		if (sscanf(buf, "%63s %511s", lv, rv) != 2)
			continue;
		if (rv[0] == '"') {
			/* if rv starts with quote, read everything up to terminating quote */
			sscanf(buf, "%*s \"%511[^\"]", rv);
		}
#ifdef DEBUG
		printf("%s %s\n", lv, rv);
#endif
		if (!strcasecmp(lv, "libpath"))
			strncpy(libpath, rv, sizeof libpath);
		if (!strcasecmp(lv, "channelid"))
			channelno = strtol(rv, NULL, 0);
		if (libpath[0] && channelno != -1) {
#ifdef DEBUG
			printf("adding rn %d Id 0x%x path %s\n", nr, channelno, libpath);
#endif
			(*callback)(nr++, channelno, libpath);
			libpath[0] = '\0';
			channelno = -1;
		}
	}

	fclose(f);
	return 0;
}
