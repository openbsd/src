#include <sys/types.h>		/* Needed by dirent.h on netbsd */
#include <stdio.h>
#include <dirent.h>

#include <linux/ext2_fs.h>

int fgetflags (const char * name, unsigned long * flags);
int fgetversion (const char * name, unsigned long * version);
int fsetflags (const char * name, unsigned long flags);
int fsetversion (const char * name, unsigned long version);
int getflags (int fd, unsigned long * flags);
int getversion (int fd, unsigned long * version);
int iterate_on_dir (const char * dir_name,
		    int (*func) (const char *, struct dirent *, void *),
		    void * private);
void list_super (struct ext2_super_block * s);
void print_fs_errors (FILE * f, unsigned short errors);
void print_flags (FILE * f, unsigned long flags, int long_format);
void print_fs_state (FILE * f, unsigned short state);
int setflags (int fd, unsigned long flags);
int setversion (int fd, unsigned long version);

int e2p_is_null_uuid(void *uu);
void e2p_uuid_to_str(void *uu, char *out);
