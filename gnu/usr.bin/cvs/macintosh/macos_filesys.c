/*
 * macos_filesys.c
 * Filesystem handling stuff for macos
 *
 * Some of this stuff is not "regular", but there are a number of weird
 * conditions that a plain filepath translation didn't seem to handle.
 * For now, this seems to work.
 *
 * Michael Ladwig <mike@twinpeaks.prc.com> --- November 1995
 */

#include <cvs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *macos_fixpath (const char *path);
static char scratchPath[1024];

int
macos_mkdir( const char *path, int oflag )
{
	char macPath[1024], *sepCh;

	strcpy( macPath, ":" );
	strcat( macPath, path );
	while( (sepCh = strchr(macPath, '/')) != NULL )
		*sepCh = ':';
	
	return mkdir(macPath);
}

int
macos_open( const char *path, int oflag, ... )
{
	char macPath[1024], *sepCh;
	
	strcpy( macPath, ":" );
	strcat( macPath, path );
	while( (sepCh = strchr(macPath, '/')) != NULL )
		*sepCh = ':';

	return open(macPath, oflag);
}

int
macos_chmod( const char *path, mode_t mode )
{
	char macPath[1024], *sepCh;
	
	strcpy( macPath, ":" );
	strcat( macPath, path );
	while( (sepCh = strchr(macPath, '/')) != NULL )
		*sepCh = ':';

	return chmod(macPath, mode);
}

int
macos_creat( const char *path, mode_t mode )
{
	char macPath[1024], *sepCh;
	
	strcpy( macPath, ":" );
	strcat( macPath, path );
	while( (sepCh = strchr(macPath, '/')) != NULL )
		*sepCh = ':';

	return creat(macPath);
}

FILE *
macos_fopen( const char *path, const char *mode )
{
	FILE	*fp;
	char	macPath[1024], *sepCh;
	
	strcpy( macPath, ":" );
	strcat( macPath, path );
	while( (sepCh = strchr(macPath, '/')) != NULL )
		*sepCh = ':';
		
	fp = fopen(macPath, mode);
	
	/* Don't know why I'm getting ENOTDIR, but it should be ENOENT */
	
	if( (fp == NULL) && (errno == ENOTDIR) ) errno = ENOENT;
	
	return fp;
}

int
macos_chdir( const char *path )
{
	char	macPath[1024], *sepCh;
	int	r;
	
	strcpy( macPath, ":" );
	strcat( macPath, path );
	while( (sepCh = strchr(macPath, '/')) != NULL )
		*sepCh = ':';

	r = chdir(macPath+1);
	if( r < 0 )
		return chdir(macPath);

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
	FILE	*fp;
	char	macPath[1024], *sepCh;

	strcpy( macPath, ":" );
	
	if( strcmp(path, ".") != 0 )
		strcat( macPath, path );
	while( (sepCh = strchr(macPath, '/')) != NULL )
		*sepCh = ':';

	return opendir( macPath );
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
		strcat( scratchPath, path );
	while( (sepCh = strchr(scratchPath, '/')) != NULL )
		*sepCh = ':';
		
	return scratchPath;
}

/* Shamelessly stolen from the OS2 port.  Oddly, only the fopen calls
	seem to respect the binary-text distinction, so I have rewritten
	the code to use fopen, fread, fwrite, and fclose instead of open.	*/
	
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