/*
 *              Author:     Bob Withers
 *              Copyright (c) 1993, All Rights Reserved
 *
 *                              NOTICE
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * The author makes no representations about the suitability of this
 * software for any purpose.  This software is provided ``as is''
 * without express or implied warranty.
 */

#ifndef DIRENT_H
#define DIRENT_H

/*  Unix style directory(3C) support for OS/2 V2.x                      */

struct dirent
{
    long            d_ino;      /* not used in this implementation      */
    long            d_off;      /* not used in this implementation      */
    unsigned short  d_namlen;
    char            d_name[1];
};


struct S_Dir
{
    char           *dirname;
    int             max_ent;
    int             tot_ent;
    int             cur_ent;
    struct dirent **entp;
};
typedef struct S_Dir            DIR;


DIR *               opendir(char *filename);
struct dirent *     readdir(DIR *dirp);
long                telldir(DIR *dirp);
void                seekdir(DIR *dirp, long loc);
void                rewinddir(DIR *dirp);
void                closedir(DIR *dirp);

#endif      /* DIRENT_H */
