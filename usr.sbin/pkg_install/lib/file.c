/*	$OpenBSD: file.c,v 1.10 2000/03/24 00:20:04 espie Exp $	*/

#ifndef lint
static const char *rcsid = "$OpenBSD: file.c,v 1.10 2000/03/24 00:20:04 espie Exp $";
#endif

/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * Miscellaneous file access utilities.
 *
 */

#include "lib.h"

#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <netdb.h>
#include <pwd.h>
#include <time.h>

/* This fixes errant package names so they end up in .tgz.
   XXX returns static storage, so beware ! Consume the result
	before reusing the function.
 */
#define TGZ	".tgz"
char *
ensure_tgz(char *name)
{
	static char buffer[FILENAME_MAX];
	size_t len;

	len = strlen(name);
	if ( (strcmp (name, "-") == 0 ) 
	     || (len >= strlen(TGZ) && strcmp(name+len-strlen(TGZ), TGZ) == 0)
	     || (len >= strlen(".tar.gz") &&  
		 strcmp(name+len-strlen(".tar.gz"), ".tar.gz") == 0)
	     || (len >= strlen(".tar") &&  
		 strcmp(name+len-strlen(".tar"), ".tar") == 0)) 
	  return name;
	else {
		snprintf(buffer, sizeof buffer, "%s%s", name, TGZ);
		return buffer;
	}
}

/* This is as ftpGetURL from FreeBSD's ftpio.c, except that it uses
 * OpenBSD's ftp command to do all FTP.
 */
static FILE *
ftpGetURL(char *url, int *retcode)
{
	FILE *ftp;
	pid_t pid_ftp;
	int p[2];

	*retcode=0;

	if (pipe(p) < 0) {
		*retcode = 1;
		return NULL;
	}

	pid_ftp = fork();
	if (pid_ftp < 0) {
		*retcode = 1;
		return NULL;
	}
	if (pid_ftp == 0) {
		/* child */
		dup2(p[1],1);
		close(p[1]);

		fprintf(stderr, ">>> ftp -o - %s\n",url); 
		execl("/usr/bin/ftp","ftp","-V","-o","-",url,NULL);
		exit(1);
	} else {
		/* parent */
		ftp = fdopen(p[0],"r");

		close(p[1]);

		if (ftp == (FILE *) NULL) {
			*retcode = 1;
			return NULL;
		}
	}
	return ftp;
}

/* Quick check to see if a file exists */
Boolean
fexists(char *fname)
{
    struct stat dummy;
    if (!lstat(fname, &dummy))
	return TRUE;
    return FALSE;
}

/* Quick check to see if something is a directory */
Boolean
isdir(char *fname)
{
    struct stat sb;

    if (lstat(fname, &sb) != FAIL && S_ISDIR(sb.st_mode))
	return TRUE;
    else
	return FALSE;
}

/* Check if something is a link to a directory */
Boolean
islinktodir(char *fname)
{
    struct stat sb;

    if (lstat(fname, &sb) != FAIL && S_ISLNK(sb.st_mode))
        if (stat(fname, &sb) != FAIL && S_ISDIR(sb.st_mode))
	    return TRUE; /* link to dir! */
        else
	    return FALSE; /* link to non-dir */
    else
        return FALSE;  /* non-link */
}

/* Check to see if file is a dir, and is empty */
Boolean
isemptydir(char *fname)
{
    if (isdir(fname) || islinktodir(fname)) {
	DIR *dirp;
	struct dirent *dp;

	dirp = opendir(fname);
	if (!dirp)
	    return FALSE;	/* no perms, leave it alone */
	for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
	    if (strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..")) {
		closedir(dirp);
		return FALSE;
	    }
	}
	(void)closedir(dirp);
	return TRUE;
    }
    return FALSE;
}

Boolean
isfile(char *fname)
{
    struct stat sb;
    if (stat(fname, &sb) != FAIL && S_ISREG(sb.st_mode))
	return TRUE;
    return FALSE;
}

/* Check to see if file is a file and is empty. If nonexistent or not
   a file, say "it's empty", otherwise return TRUE if zero sized. */
Boolean
isemptyfile(char *fname)
{
    struct stat sb;
    if (stat(fname, &sb) != FAIL && S_ISREG(sb.st_mode)) {
	if (sb.st_size != 0)
	    return FALSE;
    }
    return TRUE;
}

/* Returns TRUE if file is a URL specification */
Boolean
isURL(char *fname)
{
    /*
     * Hardcode url types... not perfect, but working.
     */
    if (!fname)
	return FALSE;
    while (isspace(*fname))
	++fname;
    if (!strncmp(fname, "ftp://", 6))
	return TRUE;
    if (!strncmp(fname, "http://", 7))
	return TRUE;
    return FALSE;
}

/* Returns the host part of a URL */
char *
fileURLHost(char *fname, char *where, int max)
{
    char *ret;

    while (isspace(*fname))
	++fname;
    /* Don't ever call this on a bad URL! */
    fname = strchr(fname, ':');
    if (fname)
    	fname+=3;
    else
    	return NULL;
    /* Do we have a place to stick our work? */
    if ((ret = where) != NULL) {
	while (*fname && *fname != '/' && max--)
	    *where++ = *fname++;
	*where = '\0';
	return ret;
    }
    /* If not, they must really want us to stomp the original string */
    ret = fname;
    while (*fname && *fname != '/')
	++fname;
    *fname = '\0';
    return ret;
}

/* Returns the filename part of a URL */
char *
fileURLFilename(char *fname, char *where, int max)
{
    char *ret;

    while (isspace(*fname))
	++fname;
    /* Don't ever call this on a bad URL! */
    fname = strchr(fname, ':');
    if (fname)
    	fname+=3;
    else
    	return NULL;
    /* Do we have a place to stick our work? */
    if ((ret = where) != NULL) {
	while (*fname && *fname != '/')
	    ++fname;
	if (*fname == '/') {
	    while (*fname && max--)
		*where++ = *fname++;
	}
	*where = '\0';
	return ret;
    }
    /* If not, they must really want us to stomp the original string */
    while (*fname && *fname != '/')
	++fname;
    return fname;
}

/*
 * Try and fetch a file by URL, returning the directory name for where
 * it's unpacked, if successful.
 */
char *
fileGetURL(char *base, char *spec)
{
    char host[MAXHOSTNAMELEN], file[FILENAME_MAX];
    char *cp, *rp;
    char fname[FILENAME_MAX];
    char pen[FILENAME_MAX];
    FILE *ftp;
    pid_t tpid;
    int i, status;
    char *hint;

    rp = NULL;
    /* Special tip that sysinstall left for us */
    hint = getenv("PKG_ADD_BASE");
    if (!isURL(spec)) {
	if (!base && !hint)
	    return NULL;
	/* We've been given an existing URL (that's known-good) and now we need
	   to construct a composite one out of that and the basename we were
	   handed as a dependency. */
	if (base) {
	    strlcpy(fname, base, sizeof fname);
	    /* OpenBSD packages are currently stored in a flat space, so
	       we don't yet need to backup the category and switch to all.
	     */
	    cp = strrchr(fname, '/');
#if 0
	    if (cp) {
		*cp = '\0';	/* chop name */
		cp = strrchr(fname, '/');
	    }
#endif
	    if (cp) {
		*(cp + 1) = '\0';
#if 0
		strcat(cp, "All/");
#endif
		strlcat(cp, ensure_tgz(spec), sizeof fname);
	    }
	    else
		return NULL;
	}
	else {
	    /* Otherwise, we've been given an environment variable hinting at the right location from sysinstall */
	    strlcpy(fname, hint, sizeof fname);
	    strlcat(fname, spec, sizeof fname);
	}
    }
    else
	strlcpy(fname, spec, sizeof fname);
    cp = fileURLHost(fname, host, MAXHOSTNAMELEN);
    if (!*cp) {
	warnx("URL `%s' has bad host part!", fname);
	return NULL;
    }

    cp = fileURLFilename(fname, file, FILENAME_MAX);
    if (!*cp) {
	warnx("URL `%s' has bad filename part!", fname);
	return NULL;
    }

    if (Verbose)
	printf("Trying to fetch %s.\n", fname);
    ftp = ftpGetURL(fname, &status);
    if (ftp) {
	pen[0] = '\0';
	if ((rp = make_playpen(pen, sizeof(pen), 0)) != NULL) {
            rp=strdup(pen); /* be safe for nested calls */
	    if (Verbose)
		printf("Extracting from FTP connection into %s\n", pen);
	    tpid = fork();
	    if (!tpid) {
		dup2(fileno(ftp), 0);
		i = execl("/bin/tar", "tar", Verbose ? "-xzvf" : "-xzf", "-", 0);
		exit(i);
	    }
	    else {
		int pstat;

		fclose(ftp);
		tpid = waitpid(tpid, &pstat, 0);
		if (Verbose)
		    printf("tar command returns %d status\n", WEXITSTATUS(pstat));
	    }
	}
	else
	    printf("Error: Unable to construct a new playpen for FTP!\n");
	fclose(ftp);
    }
    else
	printf("Error: FTP Unable to get %s: %s\n",
	       fname,
	       status ? "Error while performing FTP" :
	       hstrerror(h_errno));
    return rp;
}

char *
fileFindByPath(char *base, char *fname)
{
	static char tmp[FILENAME_MAX];
	char *cp;

	if (ispkgpattern(fname)) {
		if ((cp=findbestmatchingname(".",fname)) != NULL) {
			strcpy (tmp, cp);
			return tmp;
		}
	} else {
		strlcpy(tmp, ensure_tgz(fname), sizeof tmp);
		if (fexists(tmp) && isfile(tmp)) {
			return tmp;
		}
	}
    
	if (base) {
		strcpy(tmp, base);

		cp = strrchr(tmp, '/');
		if (cp) {
			*cp = '\0';	/* chop name */
			cp = strrchr(tmp, '/');
		}
		if (cp) {
			*(cp + 1) = '\0';
			strlcat(tmp, "All/", sizeof tmp);
			strlcat(tmp, ensure_tgz(fname), sizeof tmp);
			if (ispkgpattern(tmp)) {
				cp=findbestmatchingname(dirname_of(tmp),
							basename_of(tmp));
				if (cp) {
					char *s;
					s=strrchr(tmp,'/');
					assert(s != NULL);
					strcpy(s+1, cp);
					return tmp;
				}
			} else {
				if (fexists(tmp)) {
					return tmp;
				}
			}
		}
	}

	cp = getenv("PKG_PATH");
	while (cp) {
		char *cp2 = strsep(&cp, ":");

		snprintf(tmp, FILENAME_MAX, "%s/%s", cp2 ? cp2 : cp,
		    ensure_tgz(fname));
		if (ispkgpattern(tmp)) {
			char *s;
			s = findbestmatchingname(dirname_of(tmp),
						 basename_of(tmp));
			if (s){
				char *t;
				t=strrchr(tmp, '/');
				strcpy(t+1, s);
				return tmp;
			}
		} else {
			if (fexists(tmp) && isfile(tmp)) {
				return tmp;
			}
		}
	}
    
	return NULL;
}

char *
fileGetContents(char *fname)
{
    char *contents;
    struct stat sb;
    int fd;

    if (stat(fname, &sb) == FAIL) {
	cleanup(0);
	errx(2, "can't stat '%s'", fname);
    }

    contents = (char *)malloc((size_t)(sb.st_size) + 1);
    fd = open(fname, O_RDONLY, 0);
    if (fd == FAIL) {
	cleanup(0);
	errx(2, "unable to open '%s' for reading", fname);
    }
    if (read(fd, contents, (size_t) sb.st_size) != (size_t) sb.st_size) {
	cleanup(0);
	errx(2, "short read on '%s' - did not get %qd bytes",
			fname, (long long)sb.st_size);
    }
    close(fd);
    contents[(size_t)sb.st_size] = '\0';
    return contents;
}

/* Takes a filename and package name, returning (in "try") the canonical "preserve"
 * name for it.
 */
Boolean
make_preserve_name(char *try, size_t max, char *name, char *file)
{
    int len, i;

    if ((len = strlen(file)) == 0)
	return FALSE;
    else
	i = len - 1;
    strncpy(try, file, max);
    if (try[i] == '/') /* Catch trailing slash early and save checking in the loop */
	--i;
    for (; i; i--) {
	if (try[i] == '/') {
	    try[i + 1]= '.';
	    strncpy(&try[i + 2], &file[i + 1], max - i - 2);
	    break;
	}
    }
    if (!i) {
	try[0] = '.';
	strncpy(try + 1, file, max - 1);
    }
    /* I should probably be called rude names for these inline assignments */
    strncat(try, ".",  max -= strlen(try));
    strncat(try, name, max -= strlen(name));
    strncat(try, ".",  max--);
    strncat(try, "backup", max -= 6);
    return TRUE;
}

/* Write the contents of "str" to a file */
void
write_file(char *name, char *str)
{
	FILE	*fp;
	size_t	len;

	if ((fp = fopen(name, "w")) == (FILE *) NULL) {
		cleanup(0);
		errx(2, "cannot fopen '%s' for writing", name);
	}
	len = strlen(str);
	if (fwrite(str, 1, len, fp) != len) {
		cleanup(0);
		errx(2, "short fwrite on '%s', tried to write %d bytes",
			name, len);
	}
	if (fclose(fp)) {
		cleanup(0);
		errx(2, "failure to fclose '%s'", name);
	}
}

void
copy_file(char *dir, char *fname, char *to)
{
    char cmd[FILENAME_MAX];

    if (fname[0] == '/')
	snprintf(cmd, FILENAME_MAX, "cp -p -r %s %s", fname, to);
    else
	snprintf(cmd, FILENAME_MAX, "cp -p -r %s/%s %s", dir, fname, to);
    if (vsystem(cmd)) {
	cleanup(0);
	errx(2, "could not perform '%s'", cmd);
    }
}

void
move_file(char *dir, char *fname, char *to)
{
    char cmd[FILENAME_MAX];

    if (fname[0] == '/')
	snprintf(cmd, FILENAME_MAX, "mv %s %s", fname, to);
    else
	snprintf(cmd, FILENAME_MAX, "mv %s/%s %s", dir, fname, to);
    if (vsystem(cmd)) {
	cleanup(0);
	errx(2, "could not perform '%s'", cmd);
    }
}

/*
 * Copy a hierarchy (possibly from dir) to the current directory, or
 * if "to" is TRUE, from the current directory to a location someplace
 * else.
 *
 * Though slower, using tar to copy preserves symlinks and everything
 * without me having to write some big hairy routine to do it.
 */
void
copy_hierarchy(char *dir, char *fname, Boolean to)
{
    char cmd[FILENAME_MAX * 3];

    if (!to) {
	/* If absolute path, use it */
	if (*fname == '/')
	    dir = "/";
	snprintf(cmd, FILENAME_MAX * 3, "tar cf - -C %s %s | tar xpf -",
 		 dir, fname);
    }
    else
	snprintf(cmd, FILENAME_MAX * 3, "tar cf - %s | tar xpf - -C %s",
 		 fname, dir);
#ifdef DEBUG
    printf("Using '%s' to copy trees.\n", cmd);
#endif
    if (system(cmd)) {
	cleanup(0);
	errx(2, "copy_file: could not perform '%s'", cmd);
    }
}

/* Unpack a tar file */
int
unpack(char *pkg, char *flist)
{
    char args[10], suff[80], *cp;

    args[0] = '\0';
    /*
     * Figure out by a crude heuristic whether this or not this is probably
     * compressed.
     */
    if (strcmp(pkg, "-")) {
	cp = strrchr(pkg, '.');
	if (cp) {
	    strcpy(suff, cp + 1);
	    if (strchr(suff, 'z') || strchr(suff, 'Z'))
		strcpy(args, "-z");
	}
    }
    else
	strcpy(args, "z");
    strcat(args, "xpf");
    if (vsystem("tar %s %s %s", args, pkg, flist ? flist : "")) {
	warnx("tar extract of %s failed!", pkg);
	return 1;
    }
    return 0;
}

/*
 * Using fmt, replace all instances of:
 * 
 * %F	With the parameter "name"
 * %D	With the parameter "dir"
 * %B	Return the directory part ("base") of %D/%F
 * %f	Return the filename part of %D/%F
 * 
 */
int
format_cmd(char *buf, size_t size, const char *fmt, 
	const char *dir, const char *name)
{
	char *pos;
	size_t len;

	while (*fmt != 0 && size != 0) {
		if (*fmt == '%') {
			switch(fmt[1]) {
			case 'f':
				if (name == NULL)
					return 0;
				pos = strrchr(name, '/');
				if (pos != NULL) {
					len = strlen(name) - (pos-name);
					if (len >= size)
						return 0;
					memcpy(buf, pos, len);
					buf += len;
					size -= len;
					fmt += 2;
					continue;
				}
				/* FALLTHRU */
			case 'F':
				if (name == NULL)
					return 0;
				len = strlen(name);
				if (len >= size)
					return 0;
				memcpy(buf, name, len);
				buf += len;
				size -= len;
				fmt += 2;
				continue;
			case 'D':
				if (dir == NULL)
					return 0;
				len = strlen(dir);
				if (len >= size)
					return 0;
				memcpy(buf, dir, len);
				buf += len;
				size -= len;
				fmt += 2;
				continue;
			case 'B':
				if (dir == NULL || name == NULL)
					return 0;
				len = strlen(dir);
				if (len >= size)
					return 0;
				memcpy(buf, dir, len);
				buf += len;
				size -= len;
				if ((pos = strrchr(name, '/')) != NULL) {
					*buf++ = '/';
					size--;
					if (pos - name >= size)
						return 0;
					memcpy(buf, name, pos-name);
					buf += pos-name;
					size -= pos-name;
				}
				fmt += 2;
				continue;
			case '%':
				fmt++;	
			default:
				break;
			    
			}
		}
		*buf++ = *fmt++;
		size--;
	}
	if (size == 0)
		return 0;
	else 
	    *buf = '\0';
	return 1;
}
			
