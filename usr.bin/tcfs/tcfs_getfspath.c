/*	$OpenBSD: tcfs_getfspath.c,v 1.2 2000/06/19 20:35:47 fgsch Exp $	*/

/*
 *	Transparent Cryptographic File System (TCFS) for NetBSD 
 *	Author and mantainer: 	Luigi Catuogno [luicat@tcfs.unisa.it]
 *	
 *	references:		http://tcfs.dia.unisa.it
 *				tcfs-bsd@tcfs.unisa.it
 */

/*
 *	Base utility set v0.1
 */

#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <fstab.h>

#define WHITESPACE " \t\r\n"

int
tcfs_label_getcipher (char *label)
{
	int ciphernum;

	if (tcfs_get_label (label, NULL, &ciphernum))
	  return ciphernum;

        return (-1);
}

int
tcfs_getfspath (char *label2search, char *path)
{
	return tcfs_get_label (label2search, path, NULL);
}

int
tcfs_get_label(char *label2search, char *path, int *ciphernumber)
{
	FILE *fp;
	char *label, *line, *p, *tag, *mountpoint, *cipherfield;
	int found = 0;

	if ((fp = fopen(_PATH_FSTAB,"r")) == NULL)
		return (0);

	if ((line = calloc(1024, sizeof(char))) == NULL)
		goto out;

	while (!feof(fp) && !found) {
		p = line;
		fgets (p, 1024, fp);
		p = p + strspn(p, WHITESPACE);
		while (!found) {
			strsep(&p, WHITESPACE);  /* device */
			if (p == NULL)
				break;
			p = p + strspn(p, WHITESPACE);
			mountpoint = strsep(&p, WHITESPACE); /* mount point */
			if (p == NULL)
				break;
			tag = strsep(&p, WHITESPACE); /* file system */
			if (p == NULL || strcmp(tag, "tcfs"))
				break;

			/* find the correct label */
			label = strstr(p, "label=");
			cipherfield = strstr(p, "cipher=");
			if (label == NULL)
				break;
			p = label + 6;
			label = strsep(&p, WHITESPACE ",");
			if (!strlen(label) || strcmp(label, label2search))
				break;

			if (path) {
				strcpy(path, mountpoint);
				found = 1;
			}
			
			if (ciphernumber) {
				if (cipherfield == NULL)
					break;
				p = cipherfield + 7;
				cipherfield = strsep(&p, WHITESPACE ",");
				if (!strlen(cipherfield))
					break;

				*ciphernumber = strtol(cipherfield, &p, 0);
				if (cipherfield != p) 
					found = 1;
			}
		}
	}
	free(line);
 out:
	fclose (fp);

	return found;
}
