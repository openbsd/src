/*
 * ext2fsP.h --- private header file for ext2 library
 * 
 * Copyright (C) 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "ext2fs.h"

/*
 * Badblocks list
 */
struct ext2_struct_badblocks_list {
	int	magic;
	int	num;
	int	size;
	blk_t	*list;
	int	badblocks_flags;
};

struct ext2_struct_badblocks_iterate {
	int		magic;
	badblocks_list	bb;
	int		ptr;
};


/*
 * Directory block iterator definition
 */
struct ext2_struct_dblist {
	int			magic;
	ext2_filsys		fs;
	ino_t			size;
	ino_t			count;
	int			sorted;
	struct ext2_db_entry *	list;
};

/*
 * For directory iterators
 */
struct dir_context {
	ino_t		dir;
	int		flags;
	char		*buf;
	int (*func)(struct ext2_dir_entry *dirent,
		    int	offset,
		    int	blocksize,
		    char	*buf,
		    void	*private);
	int (*func2)(ino_t	dir,
		     int	entry,
		     struct ext2_dir_entry *dirent,
		     int	offset,
		     int	blocksize,
		     char	*buf,
		     void	*private);
	void		*private;
	errcode_t	errcode;
};

/*
 * Inode cache structure
 */
struct ext2_inode_cache {
	void *				buffer;
	blk_t				buffer_blk;
	int				cache_last;
	int				cache_size;
	int				refcount;
	struct ext2_inode_cache_ent	*cache;
};

struct ext2_inode_cache_ent {
	ino_t	ino;
	struct ext2_inode inode;
};

/* Function prototypes */

extern int ext2_process_dir_block(ext2_filsys fs,
				  blk_t	*blocknr,
				  int	blockcnt,
				  void	*private);

