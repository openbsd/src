/*
 * freefs.c --- free an ext2 filesystem
 * 
 * Copyright (C) 1993, 1994, 1995, 1996 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <linux/ext2_fs.h>

#include "ext2fsP.h"

static void ext2fs_free_inode_cache(struct ext2_inode_cache *icache);

void ext2fs_free(ext2_filsys fs)
{
	if (!fs || (fs->magic != EXT2_ET_MAGIC_EXT2FS_FILSYS))
		return;
	if (fs->io) {
		io_channel_close(fs->io);
	}
	if (fs->device_name)
		free(fs->device_name);
	if (fs->super)
		free(fs->super);
	if (fs->group_desc)
		free(fs->group_desc);
	if (fs->block_map)
		ext2fs_free_block_bitmap(fs->block_map);
	if (fs->inode_map)
		ext2fs_free_inode_bitmap(fs->inode_map);

	if (fs->badblocks)
		badblocks_list_free(fs->badblocks);
	fs->badblocks = 0;

	if (fs->dblist)
		ext2fs_free_dblist(fs->dblist);

	if (fs->icache)
		ext2fs_free_inode_cache(fs->icache);
	
	fs->magic = 0;

	free(fs);
}

void ext2fs_free_generic_bitmap(ext2fs_inode_bitmap bitmap)
{
	if (!bitmap || (bitmap->magic != EXT2_ET_MAGIC_GENERIC_BITMAP))
		return;

	bitmap->magic = 0;
	if (bitmap->description) {
		free(bitmap->description);
		bitmap->description = 0;
	}
	if (bitmap->bitmap) {
		free(bitmap->bitmap);
		bitmap->bitmap = 0;
	}
	free(bitmap);
}

void ext2fs_free_inode_bitmap(ext2fs_inode_bitmap bitmap)
{
	if (!bitmap || (bitmap->magic != EXT2_ET_MAGIC_INODE_BITMAP))
		return;

	bitmap->magic = EXT2_ET_MAGIC_GENERIC_BITMAP;
	ext2fs_free_generic_bitmap(bitmap);
}

void ext2fs_free_block_bitmap(ext2fs_block_bitmap bitmap)
{
	if (!bitmap || (bitmap->magic != EXT2_ET_MAGIC_BLOCK_BITMAP))
		return;

	bitmap->magic = EXT2_ET_MAGIC_GENERIC_BITMAP;
	ext2fs_free_generic_bitmap(bitmap);
}

/*
 * Free the inode cache structure
 */
static void ext2fs_free_inode_cache(struct ext2_inode_cache *icache)
{
	if (--icache->refcount)
		return;
	if (icache->buffer)
		free(icache->buffer);
	if (icache->cache)
		free(icache->cache);
	icache->buffer_blk = 0;
	free(icache);
}

/*
 * This procedure frees a badblocks list.
 */
void ext2fs_badblocks_list_free(ext2_badblocks_list bb)
{
	if (bb->magic != EXT2_ET_MAGIC_BADBLOCKS_LIST)
		return;

	if (bb->list)
		free(bb->list);
	bb->list = 0;
	free(bb);
}

/*
 * Free a directory block list
 */
void ext2fs_free_dblist(ext2_dblist dblist)
{
	if (!dblist || (dblist->magic != EXT2_ET_MAGIC_DBLIST))
		return;

	if (dblist->list)
		free(dblist->list);
	dblist->list = 0;
	if (dblist->fs && dblist->fs->dblist == dblist)
		dblist->fs->dblist = 0;
	dblist->magic = 0;
	free(dblist);
}

