/*
 * ldconfig - update shared library symlinks
 *
 * usage: ldconfig [-DvnNX] dir ...
 *        ldconfig -l [-Dv] lib ...
 *        ldconfig -p
 *        ldconfig -r
 *        -D: debug mode, don't update links
 *        -v: verbose mode, print things as we go
 *        -n: don't process standard directories
 *        -N: don't update the library cache
 *        -X: don't update the library links
 *        -l: library mode, manually link libraries
 *        -p: print the current library cache
 *        -r: print the current library cache in a another format
 *        -P: add path before open. used by install scripts.
 *        dir ...: directories to process
 *        lib ...: libraries to link
 *
 * Copyright 1994, 1995 David Engel and Mitch D'Souza
 *
 * This program may be used for any purpose as long as this
 * copyright notice is kept.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <a.out.h>
#include <elf_abi.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#include "../config.h"

char *___strtok = NULL;

/* For SunOS */
#ifndef PATH_MAX
#include <limits.h>
#endif

/* For SunOS */
#ifndef N_MAGIC
#define N_MAGIC(exec) ((exec).a_magic & 0xffff)
#endif

#define EXIT_OK    0
#define EXIT_FATAL 128

char *prog = NULL;
int debug = 0;			/* debug mode */
int verbose = 0;		/* verbose mode */
int libmode = 0;		/* library mode */
int nocache = 0;		/* don't build cache */
int nolinks = 0;		/* don't update links */
int merge = 0;			/* cache and link dirs on cmdline */
int Pswitch = 0;
char *Ppath = "";

char *cachefile = LDSO_CACHE;	/* default cache file */
void cache_print(void);
void cache_print_alt(void);
void cache_dolib(char *dir, char *so, char *lib, int);
void cache_rmlib(char *dir, char *so);
void cache_write(void);

char *readsoname(FILE *file);

void warn(char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "%s: warning: ", prog);

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n");

    return;
}

void error(char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "%s: ", prog);

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n");

    exit(EXIT_FATAL);
}

void *xmalloc(size_t size)
{
    void *ptr;
    if ((ptr = malloc(size)) == NULL)
	error("out of memory");
    return ptr;
}

char *xstrdup(char *str)
{
    char *ptr;
    char *strdup(const char *);
    if ((ptr = strdup(str)) == NULL)
	error("out of memory");
    return ptr;
}

/* if shared library, return a malloced copy of the soname and set the
   type, else return NULL */
char *is_shlib(char *dir, char *name, int *type)
{
    char *good = NULL;
    char *cp, *cp2;
    FILE *file;
    struct exec exec;
    Elf32_Ehdr *elf_hdr;
    struct stat statbuf;
    char buff[1024];

    /* see if name is of the form libZ.so.V */
    if (strncmp(name, "lib", 3) == 0 &&
	(cp = strstr(name, ".so.")) && cp[4])
    {
	/* find the start of the Vminor part, if any */
	if ((cp2 = strchr(cp + 4, '.')))
	    cp = cp2;
	else
	    cp = cp + strlen(cp);

	/* construct the full path name */
	snprintf(buff, sizeof buff, "%s%s%s%s", Ppath, dir,
		(*dir && strcmp(dir, "/")) ? "/" : "", name);

	/* first, make sure it's a regular file */
	if (lstat(buff, &statbuf))
	    warn("can't lstat %s (%s), skipping", buff, strerror(errno));
	else if (S_ISLNK(statbuf.st_mode) && stat(buff, &statbuf))
	{
	    if (verbose)
	        warn("%s is a stale symlink, removing", buff);
	    if (unlink(buff))
	        warn("can't unlink %s (%s), skipping", buff,
		     strerror(errno));
	}
	else if (!S_ISREG(statbuf.st_mode))
	    warn("%s is not a regular file, skipping", buff, strerror(errno));
	else
	{
	    /* then try opening it */
	    if (!(file = fopen(buff, "rb")))
		warn("can't open %s (%s), skipping", buff, strerror(errno));
	    else
	    {
		/* now make sure it's a DLL or ELF library */
		if (fread(&exec, sizeof exec, 1, file) < 1)
		    warn("can't read header from %s, skipping", buff);
		else
		{
		    elf_hdr = (Elf32_Ehdr *) &exec;
		    if (elf_hdr->e_ident[0] != 0x7f ||
			strncmp(&elf_hdr->e_ident[1], "ELF",3) != 0)
			warn("%s is not a DLL or ELF library, skipping", buff);
		    else
		    {
			if ((good = readsoname(file)) == NULL)
			    good = xstrdup(name);
			*type = LIB_ELF;
		    }
		}
		fclose(file);
	    }
	}
    }

    return good;
}

/* update the symlink to new library */
void link_shlib(char *dir, char *file, char *so)
{
    int change = -1;
    char libname[1024];
    char linkname[1024];
    struct stat libstat;
    struct stat linkstat;

    /* construct the full path names */
    snprintf(libname, sizeof libname, "%s%s/%s", Ppath, dir, file);
    snprintf(linkname, sizeof linkname, "%s/%s", dir, so);

    /* see if a link already exists */
    if (!stat(linkname, &linkstat))
    {
	/* now see if it's the one we want */
	if (stat(libname, &libstat))
	    warn("can't stat %s (%s)", libname, strerror(errno));
	else if (libstat.st_dev == linkstat.st_dev &&
	    libstat.st_ino == linkstat.st_ino)
	    change = 0;
    }

    /* then update the link, if required */
    if (change && !nolinks)
    {
	if (!lstat(linkname, &linkstat) && remove(linkname))
	    warn("can't unlink %s (%s)", linkname, strerror(errno));
	else if (symlink(file, linkname))
	    warn("can't link %s to %s (%s)", linkname, file, strerror(errno));
	else
	    change = 1;
    }

    /* some people like to know what we're doing */
    if (verbose)
	printf("\t%s => %s%s\n", so, file,
	       change < 0 ? " (SKIPPED)" :
	       (change > 0 ? " (changed)" : ""));

    return;
}

/* figure out which library is greater */
int libcmp(char *p1, char *p2)
{
    while (*p1)
    {
	if (isdigit(*p1) && isdigit(*p2))
	{
	    /* must compare this numerically */
	    int v1, v2;
	    v1 = strtoul(p1, &p1, 10);
	    v2 = strtoul(p2, &p2, 10);
	    if (v1 != v2)
		return v1 - v2;
	}
	else if (*p1 != *p2)
	    return *p1 - *p2;
	else
	    p1++, p2++;
    }

    return *p1 - *p2;
}

struct lib
{
    char *so;			/* soname of a library */
    char *name;			/* name of a library */
    struct lib *next;		/* next library in list */
};

/* update all shared library links in a directory */
void scan_dir(char *name)
{
    DIR *dir;
    struct dirent *ent;
    char *so;
    struct lib *lp, *libs = NULL;
    int libtype;
    char *op = malloc(strlen(Ppath) + strlen(name) + 1);

    /* let 'em know what's going on */
    if (verbose)
	printf("%s:\n", name);

    /* if we can't open it, we can't do anything */
    strcpy(op, Ppath);
    strcat(op, name);
    if ((dir = opendir(op)) == NULL)
    {
	warn("can't open %s (%s), skipping", name, strerror(errno));
	free(op);
	return;
    }

    /* yes, we have to look at every single file */
    while ((ent = readdir(dir)) != NULL)
    {
	/* if it's not a shared library, don't bother */
	if ((so = is_shlib(name, ent->d_name, &libtype)) == NULL)
	    continue;

	if (!nocache)
	    cache_dolib(name, so, ent->d_name, libtype);

	/* have we already seen one with the same so name? */
	for (lp = libs; lp; lp = lp->next)
	{
	    if (strcmp(so, lp->so) == 0)
	    {
	        /* we have, which one do we want to use? */
	        if (libcmp(ent->d_name, lp->name) > 0)
		{
		    /* let's use the new one */
		    free(lp->name);
		    lp->name = xstrdup(ent->d_name);
		} 
		break;
	    }
	}

	/* congratulations, you're the first one we've seen */
	if (!lp)
	{
	    lp = xmalloc(sizeof *lp);
	    lp->so = xstrdup(so);
	    lp->name = xstrdup(ent->d_name);
	    lp->next = libs;
	    libs = lp;
	}

	free(so);
    }

    /* don't need this any more */
    closedir(dir);

    /* now we have all the latest libs, update the links */
    for (lp = libs; lp; lp = lp->next)
    {
	link_shlib(op, lp->name, lp->so);
	if (strcmp(lp->name, lp->so) != 0)
	    cache_rmlib(name, lp->so);
    }

    /* always try to clean up after ourselves */
    while (libs)
    {
	lp = libs->next;
	free(libs->so);
	free(libs->name);
	free(libs);
	libs = lp;
    }
    free(op);

    return;
}

/* return the list of system-specific directories */
char *get_extpath(void)
{
    char *cp = NULL;
    FILE *file;
    struct stat stat;
    char *op = malloc(strlen(Ppath) + strlen(LDSO_CONF) + 1);

    strcpy(op, Ppath);
    strcat(op, LDSO_CONF);
    if ((file = fopen(op, "r")) != NULL)
    {
	fstat(fileno(file), &stat);
	cp = xmalloc(stat.st_size + 1);
	fread(cp, 1, stat.st_size, file);
	fclose(file);
	cp[stat.st_size] = '\0';
    }
    free(op);

    return cp;
}

int main(int argc, char **argv)
{
    int i, c;
    int nodefault = 0;
    int printcache = 0;
    char *cp, *dir, *so;
    char *extpath;
    int libtype;

    prog = argv[0];
    opterr = 0;

    while ((c = getopt(argc, argv, "DRvnNXlprf:P:m:")) != EOF)
	switch (c)
	{
	case 'D':
	    debug = 1;		/* debug mode */
	    nocache = 1;
	    nolinks = 1;
	    verbose = 1;
	    break;
	case 'R':
	    /* nothing */
	    exit(EXIT_OK);
	    break;
	case 'v':
	    verbose = 1;	/* verbose mode */
	    break;
	case 'n':
	    nodefault = 1;	/* no default dirs */
	    nocache = 1;
	    break;
	case 'N':
	    nocache = 1;	/* don't build cache */
	    break;
	case 'X':
	    nolinks = 1;	/* don't update links */
	    break;
	case 'l':
	    libmode = 1;	/* library mode */
	    break;
	case 'm':		/* Compatibility hack */
	    merge = 1;
	    nodefault = 1;
	    break;	
	case 'p':
	    printcache = 1;	/* print cache */
	    break;
	case 'r':
	    printcache = 2;	/* print cache */
	    break;
	case 'P':
	    Pswitch = 1;	/* add path before names. INSTALL */
	    Ppath = optarg;
	    break;
	default:
	    fprintf(stderr, "usage: %s [-DvnNX] dir ...\n", prog);
	    fprintf(stderr, "       %s -l [-Dv] lib ...\n", prog);
	    fprintf(stderr, "       %s -p\n", prog);
	    fprintf(stderr, "       %s -r\n", prog);
	    exit(EXIT_FATAL);
	    break;

	/* THE REST OF THESE ARE UNDOCUMENTED AND MAY BE REMOVED
	   IN FUTURE VERSIONS. */
	case 'f':
	    cachefile = optarg;	/* alternate cache file */
	    break;
	}

    /* allow me to introduce myself, hi, my name is ... */
    if (verbose)
	printf("%s: version %s\n", argv[0], VERSION);

    if (printcache)
    {
	/* print the cache -- don't you trust me? */
	if (printcache == 1) {
	    cache_print();
	} else {
	    cache_print_alt();
	}
	exit(EXIT_OK);
    }
    else if (libmode)
    {
	/* so you want to do things manually, eh? */

	/* ok, if you're so smart, which libraries do we link? */
	for (i = optind; i < argc; i++)
	{
	    /* split into directory and file parts */
	    if (!(cp = strrchr(argv[i], '/')))
	    {
		dir = "";	/* no dir, only a filename */
		cp = argv[i];
	    }
	    else
	    {
		if (cp == argv[i])
		    dir = "/";	/* file in root directory */
		else
		    dir = argv[i];
		*cp++ = '\0';	/* neither of the above */
	    }

	    /* we'd better do a little bit of checking */
	    if ((so = is_shlib(dir, cp, &libtype)) == NULL)
		error("%s%s%s is not a shared library", dir,
		      (*dir && strcmp(dir, "/")) ? "/" : "", cp);

	    /* so far, so good, maybe he knows what he's doing */
	    link_shlib(dir, cp, so);
	}
    }
    else
    {
	/* the lazy bum want's us to do all the work for him */

	/* don't cache dirs on the command line */
	int nocache_save = nocache;
	nocache = nocache || !merge;

	/* OK, which directories should we do? */
	for (i = optind; i < argc; i++)
	    scan_dir(argv[i]);

	/* restore the desired caching state */
	nocache = nocache_save;

	/* look ma, no defaults */
	if (!nodefault)
	{
	    /* I guess the defaults aren't good enough */
	    if ((extpath = get_extpath()))
	    {
		for (cp = strtok(extpath, DIR_SEP); cp;
		     cp = strtok(NULL, DIR_SEP))
		    scan_dir(cp);
		free(extpath);
	    }

	    /* everybody needs these, don't they? */
	    scan_dir("/usr/lib");
	}

	if (!nocache)
	    cache_write();
    }

    exit(EXIT_OK);
}

typedef struct liblist
{
    int flags;
    int sooffset;
    int liboffset;
    char *soname;
    char *libname;
    int skip;
    struct liblist *next;
} liblist_t;

static header_t magic = { LDSO_CACHE_MAGIC, LDSO_CACHE_VER, 0 };
static liblist_t *lib_head = NULL;

static int liblistcomp(liblist_t *x, liblist_t *y)
{
    int res;

    if ((res = libcmp(x->soname, y->soname)) == 0)
    {
        res = libcmp(strrchr(x->libname, '/') + 1,
		     strrchr(y->libname, '/') + 1);
    }

    return res;
}

void cache_dolib(char *dir, char *so, char *lib, int libtype)
{
    char fullpath[PATH_MAX];
    liblist_t *new_lib, *cur_lib;

    magic.nlibs++;
    snprintf(fullpath, sizeof fullpath, "%s/%s", dir, lib);
    new_lib = xmalloc(sizeof (liblist_t));
    new_lib->flags = libtype;
    new_lib->soname = xstrdup(so);
    new_lib->libname = xstrdup(fullpath);
    new_lib->skip = 0;

    if (lib_head == NULL || liblistcomp(new_lib, lib_head) > 0)
    {
        new_lib->next = lib_head;
	lib_head = new_lib;
    }
    else
    {
        for (cur_lib = lib_head; cur_lib->next != NULL &&
	     liblistcomp(new_lib, cur_lib->next) <= 0;
	     cur_lib = cur_lib->next)
	    /* nothing */;
	new_lib->next = cur_lib->next;
	cur_lib->next = new_lib;
    }
}

void cache_rmlib(char *dir, char *so)
{
    char fullpath[PATH_MAX];
    liblist_t *cur_lib;

    snprintf(fullpath, sizeof fullpath, "%s/%s", dir, so);

    for (cur_lib = lib_head; cur_lib != NULL; cur_lib = cur_lib->next)
    {
        if (strcmp(cur_lib->soname, so) == 0 &&
	    strcmp(cur_lib->libname, fullpath) == 0 &&
	    !cur_lib->skip)
	{
	    magic.nlibs--;
	    cur_lib->skip = 1;
	}
    }
}

void cache_write(void)
{
    int cachefd;
    int stroffset = 0;
    char tempfile[1024];
    liblist_t *cur_lib;

    if (!magic.nlibs)
	return;

    snprintf(tempfile, sizeof tempfile, "%s~", cachefile);

    if (unlink(tempfile) && errno != ENOENT)
        error("can't unlink %s (%s)", tempfile, strerror(errno));

    if ((cachefd = creat(tempfile, 0644)) < 0)
	error("can't create %s (%s)", tempfile, strerror(errno));

    write(cachefd, &magic, sizeof (header_t));

    for (cur_lib = lib_head; cur_lib != NULL; cur_lib = cur_lib->next)
    {
        if (!cur_lib->skip)
	{
	    cur_lib->sooffset = stroffset;
	    stroffset += strlen(cur_lib->soname) + 1;
	    cur_lib->liboffset = stroffset;
	    stroffset += strlen(cur_lib->libname) + 1;
	    write(cachefd, cur_lib, sizeof (libentry_t));
	}
    }

    for (cur_lib = lib_head; cur_lib != NULL; cur_lib = cur_lib->next)
    {
        if (!cur_lib->skip)
	{
	    write(cachefd, cur_lib->soname, strlen(cur_lib->soname) + 1);
	    write(cachefd, cur_lib->libname, strlen(cur_lib->libname) + 1);
	}
    }

    close(cachefd);

    if (chmod(tempfile, 0644))
	error("can't chmod %s (%s)", tempfile, strerror(errno));

    if (rename(tempfile, cachefile))
	error("can't rename %s (%s)", tempfile, strerror(errno));
}

void cache_print(void)
{
    caddr_t c;
    struct stat st;
    int fd = 0;
    char *strs;
    header_t *header;
    libentry_t *libent;

    if (stat(cachefile, &st) || (fd = open(cachefile, O_RDONLY))<0)
	error("can't read %s (%s)", cachefile, strerror(errno));
    if ((c = mmap(0,st.st_size, PROT_READ, MAP_SHARED ,fd, 0)) == (caddr_t)-1)
	error("can't map %s (%s)", cachefile, strerror(errno));
    close(fd);

    if (memcmp(((header_t *)c)->magic, LDSO_CACHE_MAGIC, LDSO_CACHE_MAGIC_LEN))
	error("%s cache corrupt", cachefile);

    if (memcmp(((header_t *)c)->version, LDSO_CACHE_VER, LDSO_CACHE_VER_LEN))
	error("wrong cache version - expected %s", LDSO_CACHE_VER);

    header = (header_t *)c;
    libent = (libentry_t *)(c + sizeof (header_t));
    strs = (char *)&libent[header->nlibs];

    printf("%d libs found in cache `%s' (version %s)\n",
	    header->nlibs, cachefile, LDSO_CACHE_VER);

    for (fd = 0; fd < header->nlibs; fd++)
    {
	printf("\t%2d - %s %s => %s\n", fd + 1, 
	libent[fd].flags == LIB_DLL ? "DLL" : "ELF",
	strs + libent[fd].sooffset, 
	strs + libent[fd].liboffset);
    }

    munmap (c,st.st_size);
}

void cache_print_alt(void)
{
    caddr_t c;
    struct stat st;
    int fd = 0;
    char *strs;
    header_t *header;
    libentry_t *libent;

    if (stat(cachefile, &st) || (fd = open(cachefile, O_RDONLY))<0)
	error("can't read %s (%s)", cachefile, strerror(errno));
    if ((c = mmap(0,st.st_size, PROT_READ, MAP_SHARED ,fd, 0)) == (caddr_t)-1)
	error("can't map %s (%s)", cachefile, strerror(errno));
    close(fd);

    if (memcmp(((header_t *)c)->magic, LDSO_CACHE_MAGIC, LDSO_CACHE_MAGIC_LEN))
	error("%s cache corrupt", cachefile);

    if (memcmp(((header_t *)c)->version, LDSO_CACHE_VER, LDSO_CACHE_VER_LEN))
	error("wrong cache version - expected %s", LDSO_CACHE_VER);

    header = (header_t *)c;
    libent = (libentry_t *)(c + sizeof (header_t));
    strs = (char *)&libent[header->nlibs];

    printf("%d libs found in cache `%s' (version %s)\n",
	    header->nlibs, cachefile, LDSO_CACHE_VER);

    for (fd = 0; fd < header->nlibs; fd++)
    {
	int i = 0, j = 0, k = 0;
	char *c,*d = NULL;

	for (c = strs + libent[fd].liboffset; *c != '\0'; c++)
	{
	    if (*c == '/') d = c;
	}
	
	if (d != NULL) {
		d = d + 4;
		for (c = d; *c != '\0'; c++)
		{
		    if (*c == '.') {
			if ((j != 0) && (k == 0)) {
			    k = i;
			} 
			if ((j == 0) && (c[1] == 's') && (c[2] == 'o')) {
			    j = i;
			}
		    }
		    i++;
		}
		printf("\t%2d:-l%*.*s%s => %s\n", fd + 1,
		j, j, d,
		d + k,
		strs + libent[fd].liboffset);	
	} else {
		printf("\t%2d:-l%s => %s\n", fd + 1,
		strs + libent[fd].sooffset,
		strs + libent[fd].liboffset);
	}
    }

    munmap (c,st.st_size);
}

