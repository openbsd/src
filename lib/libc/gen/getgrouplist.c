/*	$OpenBSD: getgrouplist.c,v 1.13 2008/06/24 14:29:45 deraadt Exp $ */
/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * get credential
 */
#include <sys/types.h>
#include <sys/limits.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <grp.h>
#include <pwd.h>

#include <rpc/rpc.h>
#include <rpcsvc/yp.h>
#include <rpcsvc/ypclnt.h>

int
getgrouplist(const char *uname, gid_t agroup, gid_t *groups, int *grpcnt)
{
	int i, ngroups = 0, ret = 0, maxgroups = *grpcnt, bail, foundyp = 0;
	extern struct group *_getgrent_yp(int *);
	struct group *grp;

	/*
	 * install primary group
	 */
	if (ngroups >= maxgroups) {
		*grpcnt = ngroups;
		return (-1);
	}
	groups[ngroups++] = agroup;

	/*
	 * Scan the group file to find additional groups.
	 */
	setgrent();
	while ((grp = _getgrent_yp(&foundyp))) {
		if (grp->gr_gid == agroup)
			continue;
		for (bail = 0, i = 0; bail == 0 && i < ngroups; i++)
			if (groups[i] == grp->gr_gid)
				bail = 1;
		if (bail)
			continue;
		for (i = 0; grp->gr_mem[i]; i++) {
			if (!strcmp(grp->gr_mem[i], uname)) {
				if (ngroups >= maxgroups) {
					ret = -1;
					goto out;
				}
				groups[ngroups++] = grp->gr_gid;
				break;
			}
		}
	}

#ifdef YP
	/*
	 * If we were told that there is a YP marker, look there now.
	 */
	if (foundyp) {
		char buf[1024], *ypdata = NULL, *key, *p;
		const char *errstr = NULL;
		static char *__ypdomain;
		struct passwd pwstore;
		int r, ypdatalen;
		gid_t gid;
		uid_t uid;
	
		if (!__ypdomain) {
			if (_yp_check(&__ypdomain) == 0) {
				goto ypout;
			}
		}

		if (getpwnam_r(uname, &pwstore, buf, sizeof buf, NULL))
			goto ypout;

		asprintf(&key, "unix.%u@%s", pwstore.pw_uid, __ypdomain);
		if (key == NULL)
			goto ypout;
		r = yp_match(__ypdomain, "netid.byname", key,
		    (int)strlen(key), &ypdata, &ypdatalen);
		free(key);
		if (r != 0)
			goto ypout;

		/* Parse the "uid:gid[,gid,gid[,...]]" string. */
		p = strchr(ypdata, ':');
		if (!p)
			goto ypout;
		*p++ = '\0';
		uid = (uid_t)strtonum(ypdata, 0, UID_MAX, &errstr);
		if (errstr || uid != pwstore.pw_uid)
			goto ypout;
		while (p && *p) {
			char *start = p;

			p = strchr(start, ',');
			if (p)
				*p++ = '\0';
			gid = (uid_t)strtonum(start, 0, GID_MAX, &errstr);
			if (errstr)
				goto ypout;

			/* Add new groups to the group list */
			for (i = 0; i < ngroups; i++) {
				if (groups[i] == gid)
					break;
			}
			if (i == ngroups) {
				if (ngroups >= maxgroups) {
					ret = -1;
					goto ypout;
				}
				groups[ngroups++] = gid;
			}
		}
ypout:
		if (ypdata)
			free(ypdata);
		goto out;
	}
#endif /* YP */

out:
	endgrent();
	*grpcnt = ngroups;
	return (ret);
}
