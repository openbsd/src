/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include <unistd.h>

#include <atypes.h>

#include <err.h>

#include <dpart.h>

RCSID("$KTH: dpart.c,v 1.6 2000/12/29 20:12:35 tol Exp $");

#ifdef MILKO_ROOT
char *dpart_root = MILKO_ROOT;
#else
char *dpart_root = "";
#endif

/*
 * Allocate a dp_part structure for partition `num' and
 * return it in `dp'.
 */

int
dp_create (u_int32_t num, struct dp_part **dp)
{
    struct dp_part *d;
    int ret;
    char str[3], *ptr = str;

    assert (dp);

    *dp = NULL;

    if (num > ('z' - 'a' * 'z' - 'a'))
	return EINVAL;

    d = malloc (sizeof (*d));
    if (d == NULL)
	return ENOMEM;
    memset (d, 0, sizeof(*d));

    if (num > 'z' - 'a') {
	ret = num / ('z' - 'a');
	num -= ret * ('z' - 'a');
	*ptr = ret + 'a';
	ptr++;
    }
    ptr[0] = num + 'a';
    ptr[1] = '\0';

    ret = asprintf (&d->part, "%s/vicep%s", dpart_root, str);
    if (ret < 0) {
	free (d);
	return EINVAL;
    }
    
    d->num = num;
    d->ref = 1;

    *dp = d;
    return 0;
}

void
dp_ref (struct dp_part *dp)
{
    if (dp)
	++dp->ref;
}

/*
 * free the dpart structure in `dp'.
 */

void
dp_free (struct dp_part *dp)
{
    if (dp) {
	if (--dp->ref)
	    return;
	if (dp->part)
	    free (dp->part);
	free (dp);
    }
}

/*
 * Iterate over all local partition and returns when one found.
 * `*dp' should one the first call be set to NULL. For each
 * call next dpart is returned. When last entry is found `*dp' is
 * set to NULL. On error != 0 is returned.
 */

int
dp_find (struct dp_part **dp)
{
    u_int32_t num;
    int found = 0, ret;
    struct stat sb;
    
    assert (dp);

    if (*dp == NULL) {
	num = -1;
    } else {	
	num = (*dp)->num;
	dp_free (*dp);
	*dp = NULL;
    }
    do { 
	++num;
	
	if (*dp != NULL)
	    dp_free (*dp);

	ret = dp_create (num, dp);
	if (ret)
	    return ret;
	
	if (stat ((*dp)->part, &sb) == 0 && S_ISDIR(sb.st_mode))
	    found = 1;

    } while (!found);

    return 0;
}


/*
 *
 */

int
dp_findvol (struct dp_part *d, void (*cb)(void *, int), void *data)
{
    DIR *dir;
    struct dirent *e;
    int fd, ret;

    assert (d);

    ret = chdir (d->part);
    if (ret)
	return 0;

    dir = opendir (d->part);
    if (dir == NULL)
	return 0;

    while ((e = readdir (dir)) != NULL) {
	if (strncmp (e->d_name, "vol", 3))
	    continue;

	fd = open (e->d_name, O_RDWR, 0600);
	if (fd < 0)
	    errx (1, "can't open %s/%s", d->part, e->d_name);

	(cb) (data, fd);
	close (fd);
    }
    closedir (dir);
    return 0;
}

/*
 *
 */

int
dp_parse (const char *str, u_int32_t *num)
{
    int len;
    u_int32_t part = 0;

    assert (str && num);

    len = strlen (str);
    if (len == 0)
	return EINVAL;
    if (len > 6 && strncmp (str, "/vicep", 6) == 0) {
	str += 6;
	len -= 6;
    }
    if (len > 2)
	return EINVAL;
    if (len == 2) {
	if (*str < 'a' || *str > 'z')
	    return EINVAL;
	part += ('z' - 'a') * (*str - 'a' + 1);
	str++;
	len--;
    }
    if (*str < 'a' || *str > 'z')
	return EINVAL;
    part += *str - 'a';

    *num = part;
    return 0;
}

/*
 *
 */

struct dp_part *
dp_getpart (const char *str)
{
    int ret;
    struct dp_part *dp;
    u_int32_t part;

    ret = dp_parse (str, &part);
    if (ret)
	return NULL;

    ret = dp_create (part, &dp);
    if (ret)
	return NULL;

    return dp;
}
