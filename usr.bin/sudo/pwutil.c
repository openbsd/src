/*
 * Copyright (c) 1996, 1998-2005, 2007-2009
 *	Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
# if defined(HAVE_MEMORY_H) && !defined(STDC_HEADERS)
#  include <memory.h>
# endif
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <pwd.h>
#include <grp.h>

#include "sudo.h"
#include "redblack.h"

#ifdef MYPW
extern void (*my_setgrent) __P((void));
extern void (*my_endgrent) __P((void));
extern struct group *(*my_getgrnam) __P((const char *));
extern struct group *(*my_getgrgid) __P((gid_t));
#define setgrent()	my_setgrent()
#define endgrent()	my_endgrent()
#define getgrnam(n)	my_getgrnam(n)
#define getgrgid(g)	my_getgrgid(g)

extern void (*my_setpwent) __P((void));
extern void (*my_endpwent) __P((void));
extern struct passwd *(*my_getpwnam) __P((const char *));
extern struct passwd *(*my_getpwuid) __P((uid_t));
#define setpwent()	my_setpwent()
#define endpwent()	my_endpwent()
#define getpwnam(n)	my_getpwnam(n)
#define getpwuid(u)	my_getpwuid(u)
#endif

/*
 * The passwd and group caches.
 */
static struct rbtree *pwcache_byuid, *pwcache_byname;
static struct rbtree *grcache_bygid, *grcache_byname;

static int  cmp_pwuid	__P((const void *, const void *));
static int  cmp_pwnam	__P((const void *, const void *));
static int  cmp_grgid	__P((const void *, const void *));
static int  cmp_grnam	__P((const void *, const void *));

/*
 * Compare by uid.
 */
static int
cmp_pwuid(v1, v2)
    const void *v1;
    const void *v2;
{
    const struct passwd *pw1 = (const struct passwd *) v1;
    const struct passwd *pw2 = (const struct passwd *) v2;
    return(pw1->pw_uid - pw2->pw_uid);
}

/*
 * Compare by user name.
 */
static int
cmp_pwnam(v1, v2)
    const void *v1;
    const void *v2;
{
    const struct passwd *pw1 = (const struct passwd *) v1;
    const struct passwd *pw2 = (const struct passwd *) v2;
    return(strcmp(pw1->pw_name, pw2->pw_name));
}

#define FIELD_SIZE(src, name, size)			\
do {							\
	if (src->name) {				\
		size = strlen(src->name) + 1;		\
		total += size;				\
	}                                               \
} while (0)

#define FIELD_COPY(src, dst, name, size)		\
do {							\
	if (src->name) {				\
		memcpy(cp, src->name, size);		\
		dst->name = cp;				\
		cp += size;				\
	}						\
} while (0)

/*
 * Dynamically allocate space for a struct password and the constituent parts
 * that we care about.  Fills in pw_passwd from shadow file.
 */
static struct passwd *
sudo_pwdup(pw)
    const struct passwd *pw;
{
    char *cp;
    const char *pw_shell;
    size_t nsize, psize, csize, gsize, dsize, ssize, total;
    struct passwd *newpw;

    /* If shell field is empty, expand to _PATH_BSHELL. */
    pw_shell = (pw->pw_shell == NULL || pw->pw_shell[0] == '\0')
	? _PATH_BSHELL : pw->pw_shell;

    /* Allocate in one big chunk for easy freeing. */
    nsize = psize = csize = gsize = dsize = ssize = 0;
    total = sizeof(struct passwd);
    FIELD_SIZE(pw, pw_name, nsize);
    FIELD_SIZE(pw, pw_passwd, psize);
#ifdef HAVE_LOGIN_CAP_H
    FIELD_SIZE(pw, pw_class, csize);
#endif
    FIELD_SIZE(pw, pw_gecos, gsize);
    FIELD_SIZE(pw, pw_dir, dsize);
    FIELD_SIZE(pw, pw_shell, ssize);

    if ((cp = malloc(total)) == NULL)
	    return(NULL);
    newpw = (struct passwd *) cp;

    /*
     * Copy in passwd contents and make strings relative to space
     * at the end of the buffer.
     */
    memcpy(newpw, pw, sizeof(struct passwd));
    cp += sizeof(struct passwd);
    FIELD_COPY(pw, newpw, pw_name, nsize);
    FIELD_COPY(pw, newpw, pw_passwd, psize);
#ifdef HAVE_LOGIN_CAP_H
    FIELD_COPY(pw, newpw, pw_class, csize);
#endif
    FIELD_COPY(pw, newpw, pw_gecos, gsize);
    FIELD_COPY(pw, newpw, pw_dir, dsize);
    FIELD_COPY(pw, newpw, pw_shell, ssize);

    return(newpw);
}

/*
 * Get a password entry by uid and allocate space for it.
 * Fills in pw_passwd from shadow file if necessary.
 */
struct passwd *
sudo_getpwuid(uid)
    uid_t uid;
{
    struct passwd key, *pw;
    struct rbnode *node;
    char *cp;

    key.pw_uid = uid;
    if ((node = rbfind(pwcache_byuid, &key)) != NULL) {
	pw = (struct passwd *) node->data;
	return(pw->pw_name != NULL ? pw : NULL);
    }
    /*
     * Cache passwd db entry if it exists or a negative response if not.
     */
    if ((pw = getpwuid(uid)) != NULL) {
	pw = sudo_pwdup(pw);
	cp = sudo_getepw(pw);		/* get shadow password */
	if (pw->pw_passwd != NULL)
	    zero_bytes(pw->pw_passwd, strlen(pw->pw_passwd));
	pw->pw_passwd = cp;
	if (rbinsert(pwcache_byuid, (void *) pw) != NULL)
	    errorx(1, "unable to cache uid %lu (%s), already exists",
		uid, pw->pw_name);
	return(pw);
    } else {
	pw = emalloc(sizeof(*pw));
	zero_bytes(pw, sizeof(*pw));
	pw->pw_uid = uid;
	if (rbinsert(pwcache_byuid, (void *) pw) != NULL)
	    errorx(1, "unable to cache uid %lu, already exists", uid);
	return(NULL);
    }
}

/*
 * Get a password entry by name and allocate space for it.
 * Fills in pw_passwd from shadow file if necessary.
 */
struct passwd *
sudo_getpwnam(name)
    const char *name;
{
    struct passwd key, *pw;
    struct rbnode *node;
    size_t len;
    char *cp;

    key.pw_name = (char *) name;
    if ((node = rbfind(pwcache_byname, &key)) != NULL) {
	pw = (struct passwd *) node->data;
	return(pw->pw_uid != (uid_t) -1 ? pw : NULL);
    }
    /*
     * Cache passwd db entry if it exists or a negative response if not.
     */
    if ((pw = getpwnam(name)) != NULL) {
	pw = sudo_pwdup(pw);
	cp = sudo_getepw(pw);		/* get shadow password */
	if (pw->pw_passwd != NULL)
	    zero_bytes(pw->pw_passwd, strlen(pw->pw_passwd));
	pw->pw_passwd = cp;
	if (rbinsert(pwcache_byname, (void *) pw) != NULL)
	    errorx(1, "unable to cache user %s, already exists", name);
	return(pw);
    } else {
	len = strlen(name) + 1;
	cp = emalloc(sizeof(*pw) + len);
	zero_bytes(cp, sizeof(*pw));
	pw = (struct passwd *) cp;
	cp += sizeof(*pw);
	memcpy(cp, name, len);
	pw->pw_name = cp;
	pw->pw_uid = (uid_t) -1;
	if (rbinsert(pwcache_byname, (void *) pw) != NULL)
	    errorx(1, "unable to cache user %s, already exists", name);
	return(NULL);
    }
}

/*
 * Take a uid in string form "#123" and return a faked up passwd struct.
 */
struct passwd *
sudo_fakepwnam(user, gid)
    const char *user;
    gid_t gid;
{
    struct passwd *pw;
    struct rbnode *node;
    size_t len;

    len = strlen(user);
    pw = emalloc(sizeof(struct passwd) + len + 1 /* pw_name */ +
	sizeof("*") /* pw_passwd */ + sizeof("") /* pw_gecos */ +
	sizeof("/") /* pw_dir */ + sizeof(_PATH_BSHELL));
    zero_bytes(pw, sizeof(struct passwd));
    pw->pw_uid = (uid_t) atoi(user + 1);
    pw->pw_gid = gid;
    pw->pw_name = (char *)pw + sizeof(struct passwd);
    memcpy(pw->pw_name, user, len + 1);
    pw->pw_passwd = pw->pw_name + len + 1;
    memcpy(pw->pw_passwd, "*", 2);
    pw->pw_gecos = pw->pw_passwd + 2;
    pw->pw_gecos[0] = '\0';
    pw->pw_dir = pw->pw_gecos + 1;
    memcpy(pw->pw_dir, "/", 2);
    pw->pw_shell = pw->pw_dir + 2;
    memcpy(pw->pw_shell, _PATH_BSHELL, sizeof(_PATH_BSHELL));

    /* Store by uid and by name, overwriting cached version. */
    if ((node = rbinsert(pwcache_byuid, pw)) != NULL) {
	efree(node->data);
	node->data = (void *) pw;
    }
    if ((node = rbinsert(pwcache_byname, pw)) != NULL) {
	efree(node->data);
	node->data = (void *) pw;
    }
    return(pw);
}

/*
 * Take a gid in string form "#123" and return a faked up group struct.
 */
struct group *
sudo_fakegrnam(group)
    const char *group;
{
    struct group *gr;
    struct rbnode *node;
    size_t len;

    len = strlen(group);
    gr = emalloc(sizeof(struct group) + len + 1);
    zero_bytes(gr, sizeof(struct group));
    gr->gr_gid = (gid_t) atoi(group + 1);
    gr->gr_name = (char *)gr + sizeof(struct group);
    strlcpy(gr->gr_name, group, len + 1);

    /* Store by gid and by name, overwriting cached version. */
    if ((node = rbinsert(grcache_bygid, gr)) != NULL) {
	efree(node->data);
	node->data = (void *) gr;
    }
    if ((node = rbinsert(grcache_byname, gr)) != NULL) {
	efree(node->data);
	node->data = (void *) gr;
    }
    return(gr);
}

void
sudo_setpwent()
{
    setpwent();
    sudo_setspent();
    if (pwcache_byuid == NULL)
	pwcache_byuid = rbcreate(cmp_pwuid);
    if (pwcache_byname == NULL)
	pwcache_byname = rbcreate(cmp_pwnam);
}

#ifdef PURIFY
static void pw_free	__P((void *));

void
sudo_freepwcache()
{
    if (pwcache_byuid != NULL) {
	rbdestroy(pwcache_byuid, pw_free);
	pwcache_byuid = NULL;
    }
    if (pwcache_byname != NULL) {
	rbdestroy(pwcache_byname, NULL);
	pwcache_byname = NULL;
    }
}

static void
pw_free(v)
    void *v;
{
    struct passwd *pw = (struct passwd *) v;

    if (pw->pw_passwd != NULL) {
	zero_bytes(pw->pw_passwd, strlen(pw->pw_passwd));
	efree(pw->pw_passwd);
    }
    efree(pw);
}
#endif /* PURIFY */

void
sudo_endpwent()
{
    endpwent();
    sudo_endspent();
#ifdef PURIFY
    sudo_freepwcache();
#endif
}

/*
 * Compare by gid.
 */
static int
cmp_grgid(v1, v2)
    const void *v1;
    const void *v2;
{
    const struct group *grp1 = (const struct group *) v1;
    const struct group *grp2 = (const struct group *) v2;
    return(grp1->gr_gid - grp2->gr_gid);
}

/*
 * Compare by group name.
 */
static int
cmp_grnam(v1, v2)
    const void *v1;
    const void *v2;
{
    const struct group *grp1 = (const struct group *) v1;
    const struct group *grp2 = (const struct group *) v2;
    return(strcmp(grp1->gr_name, grp2->gr_name));
}

struct group *
sudo_grdup(gr)
    const struct group *gr;
{
    char *cp;
    size_t nsize, psize, nmem, total, len;
    struct group *newgr;

    /* Allocate in one big chunk for easy freeing. */
    nsize = psize = nmem = 0;
    total = sizeof(struct group);
    FIELD_SIZE(gr, gr_name, nsize);
    FIELD_SIZE(gr, gr_passwd, psize);
    if (gr->gr_mem) {
	for (nmem = 0; gr->gr_mem[nmem] != NULL; nmem++)
	    total += strlen(gr->gr_mem[nmem]) + 1;
	nmem++;
	total += sizeof(char *) * nmem;
    }
    if ((cp = malloc(total)) == NULL)
	    return(NULL);
    newgr = (struct group *)cp;

    /*
     * Copy in group contents and make strings relative to space
     * at the end of the buffer.  Note that gr_mem must come
     * immediately after struct group to guarantee proper alignment.
     */
    (void)memcpy(newgr, gr, sizeof(struct group));
    cp += sizeof(struct group);
    if (gr->gr_mem) {
	newgr->gr_mem = (char **)cp;
	cp += sizeof(char *) * nmem;
	for (nmem = 0; gr->gr_mem[nmem] != NULL; nmem++) {
	    len = strlen(gr->gr_mem[nmem]) + 1;
	    memcpy(cp, gr->gr_mem[nmem], len);
	    newgr->gr_mem[nmem] = cp;
	    cp += len;
	}
	newgr->gr_mem[nmem] = NULL;
    }
    FIELD_COPY(gr, newgr, gr_passwd, psize);
    FIELD_COPY(gr, newgr, gr_name, nsize);

    return(newgr);
}

/*
 * Get a group entry by gid and allocate space for it.
 */
struct group *
sudo_getgrgid(gid)
    gid_t gid;
{
    struct group key, *gr;
    struct rbnode *node;

    key.gr_gid = gid;
    if ((node = rbfind(grcache_bygid, &key)) != NULL) {
	gr = (struct group *) node->data;
	return(gr->gr_name != NULL ? gr : NULL);
    }
    /*
     * Cache group db entry if it exists or a negative response if not.
     */
    if ((gr = getgrgid(gid)) != NULL) {
	gr = sudo_grdup(gr);
	if (rbinsert(grcache_bygid, (void *) gr) != NULL)
	    errorx(1, "unable to cache gid %lu (%s), already exists",
		gid, gr->gr_name);
	return(gr);
    } else {
	gr = emalloc(sizeof(*gr));
	zero_bytes(gr, sizeof(*gr));
	gr->gr_gid = gid;
	if (rbinsert(grcache_bygid, (void *) gr) != NULL)
	    errorx(1, "unable to cache gid %lu, already exists, gid");
	return(NULL);
    }
}

/*
 * Get a group entry by name and allocate space for it.
 */
struct group *
sudo_getgrnam(name)
    const char *name;
{
    struct group key, *gr;
    struct rbnode *node;
    size_t len;
    char *cp;

    key.gr_name = (char *) name;
    if ((node = rbfind(grcache_byname, &key)) != NULL) {
	gr = (struct group *) node->data;
	return(gr->gr_gid != (gid_t) -1 ? gr : NULL);
    }
    /*
     * Cache group db entry if it exists or a negative response if not.
     */
    if ((gr = getgrnam(name)) != NULL) {
	gr = sudo_grdup(gr);
	if (rbinsert(grcache_byname, (void *) gr) != NULL)
	    errorx(1, "unable to cache group %s, already exists", name);
	return(gr);
    } else {
	len = strlen(name) + 1;
	cp = emalloc(sizeof(*gr) + len);
	zero_bytes(cp, sizeof(*gr));
	gr = (struct group *) cp;
	cp += sizeof(*gr);
	memcpy(cp, name, len);
	gr->gr_name = cp;
	gr->gr_gid = (gid_t) -1;
	if (rbinsert(grcache_byname, (void *) gr) != NULL)
	    errorx(1, "unable to cache group %s, already exists", name);
	return(NULL);
    }
}

void
sudo_setgrent()
{
    setgrent();
    if (grcache_bygid == NULL)
	grcache_bygid = rbcreate(cmp_grgid);
    if (grcache_byname == NULL)
	grcache_byname = rbcreate(cmp_grnam);
}

#ifdef PURIFY
void
sudo_freegrcache()
{
    if (grcache_bygid != NULL) {
	rbdestroy(grcache_bygid, free);
	grcache_bygid = NULL;
    }
    if (grcache_byname != NULL) {
	rbdestroy(grcache_byname, NULL);
	grcache_byname = NULL;
    }
}
#endif /* PURIFY */

void
sudo_endgrent()
{
    endgrent();
#ifdef PURIFY
    sudo_freegrcache();
#endif
}
