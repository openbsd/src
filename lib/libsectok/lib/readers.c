/*
 * parse the reader.conf file
 *
 * Jim Rees
 * University of Michigan CITI, August 2000
 */
static char *rcsid = "$Id: readers.c,v 1.1 2001/05/22 15:35:58 rees Exp $";

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int
DBUpdateReaders(char *readerconf, void (callback) (char *name, unsigned long channelId, char *driverFile))
{
	FILE *f;
	char buf[512], lv[64], rv[512], libpath[512];
	long channelno;

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
			printf("adding %x %s\n", channelno, libpath);
#endif
			(*callback)("", channelno, libpath);
			libpath[0] = '\0';
			channelno = -1;
		}
	}

	fclose(f);
	return 0;
}
