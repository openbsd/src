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

#include <stdlib.h>
#include <string.h>

#define INCL_DOSFILEMGR
#define INCL_DOSERRORS
#include <os2.h>

#include "dirent.h"


#define DIRENT_INCR             25


DIR *opendir(char *filename)
{
    auto     size_t             len;
    auto     DIR               *dirp;
    auto     char              *p;
    auto     HDIR               hdir;

#ifdef OS2_16
    auto     USHORT             rc;         /* for 16 bit OS/2          */
    auto     FILEFINDBUF        ff;
    auto     USHORT             cnt;
#else
    auto     APIRET             rc;         /* for 32 bit OS/2          */
    auto     FILEFINDBUF3       ff;
    auto     ULONG              cnt;
#endif  /* OS2_16 */

    if (NULL == filename || '\0' == filename[0])
        filename = ".";

    dirp = malloc(sizeof(*dirp));
    if (NULL == dirp)
        return(NULL);

    len = strlen(filename);
    dirp->dirname = malloc(len + 5);
    if (NULL == dirp->dirname)
    {
        free(dirp);
        return(NULL);
    }

    dirp->max_ent = 0;
    dirp->tot_ent = 0;
    dirp->cur_ent = 0;
    dirp->entp    = NULL;
    strcpy(dirp->dirname, filename);
    for (p = dirp->dirname; *p; ++p)
    {
        if ('/' == *p)
            *p = '\\';
    }

    if ('\\' != dirp->dirname[len - 1])
        strcat(dirp->dirname, "\\");

    strcat(dirp->dirname, "*.*");

    hdir = HDIR_SYSTEM;
    cnt  = 1;
    rc = DosFindFirst(dirp->dirname, &hdir,
                      FILE_NORMAL | FILE_READONLY | FILE_HIDDEN |
                      FILE_SYSTEM | FILE_DIRECTORY | FILE_ARCHIVED,
                      &ff, sizeof(ff), &cnt, FIL_STANDARD);

    while (NO_ERROR == rc)
    {
        auto     struct dirent     *entp;

        if (dirp->tot_ent >= dirp->max_ent)
        {
            auto     struct dirent    **p;

            dirp->max_ent += DIRENT_INCR;
            p = realloc(dirp->entp, dirp->max_ent * sizeof(entp));
            if (NULL == p)
            {
                rc = ERROR_NOT_ENOUGH_MEMORY;
                break;
            }

            dirp->entp = p;
        }

        entp = malloc(sizeof(*entp) + (size_t) ff.cchName);
        if (NULL == entp)
        {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            break;
        }

        entp->d_ino = 0;
        entp->d_off = dirp->tot_ent;
        entp->d_namlen = (unsigned short) ff.cchName;
        memcpy(entp->d_name, ff.achName, entp->d_namlen);
        entp->d_name[entp->d_namlen] = '\0';
        dirp->entp[dirp->tot_ent++] = entp;

        cnt = 1;
        rc = DosFindNext(hdir, &ff, sizeof(ff), &cnt);
    }

    DosFindClose(hdir);
    if (ERROR_NO_MORE_FILES == rc)
        return(dirp);

    closedir(dirp);
    return(NULL);
}


struct dirent *readdir(DIR *dirp)
{
    if (dirp->cur_ent < 0 || dirp->cur_ent >= dirp->tot_ent)
        return(NULL);

    return(dirp->entp[dirp->cur_ent++]);
}


long telldir(DIR *dirp)
{
    return((long) dirp->cur_ent);
}


void seekdir(DIR *dirp, long loc)
{
    dirp->cur_ent = (int) loc;
    return;
}


void rewinddir(DIR *dirp)
{
    dirp->cur_ent = 0;
    return;
}


void closedir(DIR *dirp)
{
    if (dirp)
    {
        if (dirp->dirname)
            free(dirp->dirname);

        if (dirp->entp)
        {
            register int        i;

            for (i = 0; i < dirp->tot_ent; ++i)
                free(dirp->entp[i]);

            free(dirp->entp);
        }
    }

    return;
}
