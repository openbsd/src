/*
 * unix_io.c --- This is the Unix I/O interface to the I/O manager.
 *
 * Implements a one-block write-through cache.
 *
 * Copyright (C) 1993, 1994, 1995 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#if HAVE_ERRNO_H
#include <errno.h>
#endif

#include "et/com_err.h"
#include "ext2fs/ext2_err.h"
#include "io.h"

/*
 * For checking structure magic numbers...
 */

#define EXT2_CHECK_MAGIC(struct, code) \
	  if ((struct)->magic != (code)) return (code)
  
struct unix_private_data {
	int	magic;
	int	dev;
	int	flags;
	char	*buf;
	int	buf_block_nr;
};

static errcode_t unix_open(const char *name, int flags, io_channel *channel);
static errcode_t unix_close(io_channel channel);
static errcode_t unix_set_blksize(io_channel channel, int blksize);
static errcode_t unix_read_blk(io_channel channel, unsigned long block,
			       int count, void *data);
static errcode_t unix_write_blk(io_channel channel, unsigned long block,
				int count, const void *data);
static errcode_t unix_flush(io_channel channel);

static struct struct_io_manager struct_unix_manager = {
	EXT2_ET_MAGIC_IO_MANAGER,
	"Unix I/O Manager",
	unix_open,
	unix_close,
	unix_set_blksize,
	unix_read_blk,
	unix_write_blk,
	unix_flush
};

io_manager unix_io_manager = &struct_unix_manager;

static errcode_t unix_open(const char *name, int flags, io_channel *channel)
{
	io_channel	io = NULL;
	struct unix_private_data *data = NULL;
	errcode_t	retval;

	if (name == 0)
		return EXT2_ET_BAD_DEVICE_NAME;
	io = (io_channel) malloc(sizeof(struct struct_io_channel));
	if (!io)
		return ENOMEM;
	memset(io, 0, sizeof(struct struct_io_channel));
	io->magic = EXT2_ET_MAGIC_IO_CHANNEL;
	data = (struct unix_private_data *)
		malloc(sizeof(struct unix_private_data));
	if (!data) {
		retval = ENOMEM;
		goto cleanup;
	}
	io->manager = unix_io_manager;
	io->name = malloc(strlen(name)+1);
	if (!io->name) {
		retval = ENOMEM;
		goto cleanup;
	}
	strcpy(io->name, name);
	io->private_data = data;
	io->block_size = 1024;
	io->read_error = 0;
	io->write_error = 0;
	io->refcount = 1;

	memset(data, 0, sizeof(struct unix_private_data));
	data->magic = EXT2_ET_MAGIC_UNIX_IO_CHANNEL;
	data->buf = malloc(io->block_size);
	data->buf_block_nr = -1;
	if (!data->buf) {
		retval = ENOMEM;
		goto cleanup;
	}
	data->dev = open(name, (flags & IO_FLAG_RW) ? O_RDWR : O_RDONLY);
	if (data->dev < 0) {
		retval = errno;
		goto cleanup;
	}
	*channel = io;
	return 0;

cleanup:
	if (io)
		free(io);
	if (data) {
		if (data->buf)
			free(data->buf);
		free(data);
	}
	return retval;
}

static errcode_t unix_close(io_channel channel)
{
	struct unix_private_data *data;
	errcode_t	retval = 0;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct unix_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

	if (--channel->refcount > 0)
		return 0;
	
	if (close(data->dev) < 0)
		retval = errno;
	if (data->buf)
		free(data->buf);
	if (channel->private_data)
		free(channel->private_data);
	if (channel->name)
		free(channel->name);
	free(channel);
	return retval;
}

static errcode_t unix_set_blksize(io_channel channel, int blksize)
{
	struct unix_private_data *data;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct unix_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

	if (channel->block_size != blksize) {
		channel->block_size = blksize;
		free(data->buf);
		data->buf = malloc(blksize);
		if (!data->buf)
			return ENOMEM;
		data->buf_block_nr = -1;
	}
	return 0;
}


static errcode_t unix_read_blk(io_channel channel, unsigned long block,
			       int count, void *buf)
{
	struct unix_private_data *data;
	errcode_t	retval;
	size_t		size;
	ext2_loff_t	location;
	int		actual = 0;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct unix_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

	/*
	 * If it's in the cache, use it!
	 */
	if ((count == 1) && (block == data->buf_block_nr)) {
		memcpy(buf, data->buf, channel->block_size);
		return 0;
	}
#if 0
	printf("read_block %lu (%d)\n", block, count);
#endif
	size = (count < 0) ? -count : count * channel->block_size;
	location = (ext2_loff_t) block * channel->block_size;
	if (ext2fs_llseek(data->dev, location, SEEK_SET) != location) {
		retval = errno;
		goto error_out;
	}
	actual = read(data->dev, buf, size);
	if (actual != size) {
		if (actual < 0)
			actual = 0;
		retval = EXT2_ET_SHORT_READ;
		goto error_out;
	}
	if (count == 1) {
		data->buf_block_nr = block;
		memcpy(data->buf, buf, size);	/* Update the cache */
	}
	return 0;
	
error_out:
	memset((char *) buf+actual, 0, size-actual);
	if (channel->read_error)
		retval = (channel->read_error)(channel, block, count, buf,
					       size, actual, retval);
	return retval;
}

static errcode_t unix_write_blk(io_channel channel, unsigned long block,
				int count, const void *buf)
{
	struct unix_private_data *data;
	size_t		size;
	ext2_loff_t	location;
	int		actual = 0;
	errcode_t	retval;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct unix_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

	if (count == 1)
		size = channel->block_size;
	else {
		data->buf_block_nr = -1; 	/* Invalidate the cache */
		if (count < 0)
			size = -count;
		else
			size = count * channel->block_size;
	} 

	location = (ext2_loff_t) block * channel->block_size;
	if (ext2fs_llseek(data->dev, location, SEEK_SET) != location) {
		retval = errno;
		goto error_out;
	}
	
	actual = write(data->dev, buf, size);
	if (actual != size) {
		retval = EXT2_ET_SHORT_WRITE;
		goto error_out;
	}

	if ((count == 1) && (block == data->buf_block_nr))
		memcpy(data->buf, buf, size); /* Update the cache */
	
	return 0;
	
error_out:
	if (channel->write_error)
		retval = (channel->write_error)(channel, block, count, buf,
						size, actual, retval);
	return retval;
}

/*
 * Flush data buffers to disk.  Since we are currently using a
 * write-through cache, this is a no-op.
 */
static errcode_t unix_flush(io_channel channel)
{
	struct unix_private_data *data;
	
	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct unix_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);
	
	return 0;
}

