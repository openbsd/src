/*
 * debugfs.h --- header file for the debugfs program
 */

#include <linux/ext2_fs.h>
#include "ext2fs/ext2fs.h"

#ifdef __STDC__
#define NOARGS void
#else
#define NOARGS
#define const
#endif

extern ext2_filsys current_fs;
extern ino_t	root, cwd;

extern FILE *open_pager(void);
extern void close_pager(FILE *stream);
extern int check_fs_open(char *name);
extern int check_fs_not_open(char *name);
extern int check_fs_read_write(char *name);
extern ino_t string_to_inode(char *str);
extern char *time_to_string(__u32);

/* ss command functions */

/* dump.c */
extern void do_dump(int argc, char **argv);
extern void do_cat(int argc, char **argv);

/* lsdel.c */
extern void do_lsdel(int argc, char **argv);

/* icheck.c */
extern void do_icheck(int argc, char **argv);

/* ncheck.c */
extern void do_ncheck(int argc, char **argv);

/* debugfs.c */

extern void do_open_filesys(int argc, char **argv);
extern void do_close_filesys(int argc, char **argv);
extern void do_init_filesys(int argc, char **argv);
extern void do_show_super_stats(int argc, char **argv);
extern void do_kill_file(int argc, char **argv);
extern void do_rm(int argc, char **argv);
extern void do_link(int argc, char **argv);
extern void do_unlink(int argc, char **argv);
extern void do_find_free_block(int argc, char **argv);
extern void do_find_free_inode(int argc, char **argv);
extern void do_stat(int argc, char **argv);

extern void do_chroot(int argc, char **argv);
extern void do_clri(int argc, char **argv);
extern void do_freei(int argc, char **argv);
extern void do_seti(int argc, char **argv);
extern void do_testi(int argc, char **argv);
extern void do_freeb(int argc, char **argv);
extern void do_setb(int argc, char **argv);
extern void do_testb(int argc, char **argv);
extern void do_modify_inode(int argc, char **argv);
extern void do_list_dir(int argc, char **argv);
extern void do_change_working_dir(int argc, char **argv);
extern void do_print_working_directory(int argc, char **argv);
extern void do_write(int argc, char **argv);
extern void do_mknod(int argc, char **argv);
extern void do_mkdir(int argc, char **argv);
extern void do_rmdir(int argc, char **argv);
extern void do_show_debugfs_params(int argc, char **argv);
extern void do_expand_dir(int argc, char **argv);







