// dirent.h

// djl
// Provide UNIX compatibility

#ifndef  _INC_DIRENT
#define  _INC_DIRENT

//
// NT versions of readdir(), etc
// From the MSDOS implementation
//

// Directory entry size 
#ifdef DIRSIZ
#undef DIRSIZ
#endif
#define DIRSIZ(rp)  (sizeof(struct direct))

// needed to compile directory stuff
#define DIRENT direct

// structure of a directory entry
typedef struct direct 
{
	long	d_ino;			// inode number (not used by MS-DOS) 
	int	d_namlen;		// Name length 
	char	d_name[257];		// file name 
} _DIRECT;

// structure for dir operations 
typedef struct _dir_struc
{
	char	*start;			// Starting position
	char	*curr;			// Current position
	long	size;			// Size of string table
	long	nfiles;			// number if filenames in table
	struct direct dirstr;		// Directory structure to return
} DIR;

DIR *		win32_opendir(char *filename);
struct direct *	win32_readdir(DIR *dirp);
long		win32_telldir(DIR *dirp);
void		win32_seekdir(DIR *dirp,long loc);
void		win32_rewinddir(DIR *dirp);
int		win32_closedir(DIR *dirp);


#endif //_INC_DIRENT
