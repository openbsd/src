/*
 * macos_filesys.c
 * Filesystem handling stuff for macos
 *
 * Michael Ladwig <mike@twinpeaks.prc.com> --- November 1995 (Initial version)
 *														 --- May 1996 (Handled relative paths better)
 *														 --- June 1996 (Have open look absolute and relative)
 */

#include "mac_config.h"
#include <config.h>
#include <system.h>

#ifdef MSL_LIBRARY
#include <errno.h>
#else
#include <sys/errno.h>
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *macos_fixpath (const char *path);
char scratchPath[1024];

int
macos_mkdir( const char *path, int oflag )
{
	return mkdir( macos_fixpath(path) );
}

int
macos_open( const char *path, int oflag, ... )
{
	int	r;
	char	*macPath;
	
	macPath = macos_fixpath(path);

	r = open( macPath, oflag );
	if( r < 0 ) return open( macPath+1, oflag );

	return r;
}

int
macos_chmod( const char *path, mode_t mode )
{
	return chmod(macos_fixpath(path), mode);
}

int
macos_creat( const char *path, mode_t mode )
{
	return creat( macos_fixpath(path) );
}

FILE *
macos_fopen( const char *path, const char *mode )
{
	FILE	*fp;
		
	fp = fopen(macos_fixpath(path), mode);
	
	/* I'm getting ENOTDIR, CVS expects ENOENT */
	
	if( (fp == NULL) && (errno == ENOTDIR) ) errno = ENOENT;
	
	return fp;
}

int
macos_chdir( const char *path )
{
	int	r;
	char	*macPath;
	
	macPath = macos_fixpath(path);

	r = chdir(macPath+1);
	if( r < 0 ) return chdir(macPath);

	return r;
}

int
macos_access(const char *path, int amode)
{	
	return access( macos_fixpath(path), amode );
}

DIR *
macos_opendir(const char *path)
{
	return opendir( macos_fixpath(path) );
}

int
macos_stat (const char *path, struct stat *ststr)
{
	return stat( macos_fixpath(path), ststr );
}

int
macos_rename (const char *path, const char *newpath)
{
	char	macPath_from[1024], macPath_to[1024];

	strcpy( macPath_from, macos_fixpath(path) );
	strcpy( macPath_to, macos_fixpath(newpath) );

	return rename( macPath_from, macPath_to );
}

int
macos_rmdir (const char *path)
{
	return rmdir( macos_fixpath(path) );
}

int
macos_unlink (const char *path)
{
	return unlink( macos_fixpath(path) );
}

char *
macos_fixpath (const char *path)
{
	char		*sepCh;
	
	strcpy( scratchPath, ":" );
	
	if( (*path == '.') && (*(path+1) == '/') )
		strcat( scratchPath, path+2 );
	else
	{
		if( strcmp(path, ".") != 0 )
			strcat( scratchPath, path );
	}
	while( (sepCh = strchr(scratchPath, '/')) != NULL )
		*sepCh = ':';
		
	//fprintf(stderr,"MacOS fixpath <%s>", path);
	//fprintf(stderr," -> <%s>\n", scratchPath);
		
	return scratchPath;
}

/*
 * I intended to shamelessly steal from the OS2 port.  Oddly, only the
 * fopen calls seem to respect the binary-text distinction, so I have
 * rewritten the code to use fopen, fread, fwrite, and fclose instead of
 * open, read, write, and close
 */
	
void
convert_file (char *infile,  int inflags,
	      char *outfile, int outflags)
{
    FILE *infd, *outfd;
    char buf[8192];
    int len;
    char iflags[10], oflags[10];

    if( inflags & OPEN_BINARY )
    	strcpy( iflags, "rb" );
    else
    	strcpy( iflags, "r" );
    	
    if( outflags & OPEN_BINARY )
    	strcpy( oflags, "wb" );
    else
    	strcpy( oflags, "w" );

    if ((infd = CVS_FOPEN (infile, iflags)) == NULL)
        error (1, errno, "couldn't read %s", infile);
    if ((outfd = CVS_FOPEN (outfile, oflags)) == NULL)
        error (1, errno, "couldn't write %s", outfile);

    while ((len = fread (buf, sizeof (char), sizeof (buf), infd)) > 0)
        if (fwrite (buf, sizeof (char), len, outfd) < 0)
	    error (1, errno, "error writing %s", outfile);
    if (len < 0)
        error (1, errno, "error reading %s", infile);

    if (fclose (outfd) < 0)
        error (0, errno, "warning: couldn't close %s", outfile);
    if (fclose (infd) < 0)
        error (0, errno, "warning: couldn't close %s", infile);
}

