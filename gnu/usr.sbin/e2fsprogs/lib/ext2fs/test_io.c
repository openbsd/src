/*
 * test_io.c --- This is the Test I/O interface.
 *
 * Copyright (C) 1996 Theodore Ts'o.
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
  
struct test_private_data {
	int	magic;
	io_channel real;
	void (*read_blk)(unsigned long block, int count, errcode_t err);
	void (*write_blk)(unsigned long block, int count, errcode_t err);
	void (*set_blksize)(int blksize, errcode_t err);
};

static errcode_t test_open(const char *name, int flags, io_channel *channel);
static errcode_t test_close(io_channel channel);
static errcode_t test_set_blksize(io_channel channel, int blksize);
static errcode_t test_read_blk(io_channel channel, unsigned long block,
			       int count, void *data);
static errcode_t test_write_blk(io_channel channel, unsigned long block,
				int count, const void *data);
static errcode_t test_flush(io_channel channel);

static struct struct_io_manager struct_test_manager = {
	EXT2_ET_MAGIC_IO_MANAGER,
	"Test I/O Manager",
	test_open,
	test_close,
	test_set_blksize,
	test_read_blk,
	test_write_blk,
	test_flush
};

io_manager test_io_manager = &struct_test_manager;

/*
 * These global variable can be set by the test program as
 * necessary *before* calling test_open
 */
io_manager test_io_backing_manager = 0;
void (*test_io_cb_read_blk)
	(unsigned long block, int count, errcode_t err) = 0;
void (*test_io_cb_write_blk)
	(unsigned long block, int count, errcode_t err) = 0;
void (*test_io_cb_set_blksize)
	(int blksize, errcode_t err) = 0;

static errcode_t test_open(const char *name, int flags, io_channel *channel)
{
	io_channel	io = NULL;
	struct test_private_data *data = NULL;
	errcode_t	retval;

	if (name == 0)
		return EXT2_ET_BAD_DEVICE_NAME;
	io = (io_channel) malloc(sizeof(struct struct_io_channel));
	if (!io)
		return ENOMEM;
	memset(io, 0, sizeof(struct struct_io_channel));
	io->magic = EXT2_ET_MAGIC_IO_CHANNEL;
	data = (struct test_private_data *)
		malloc(sizeof(struct test_private_data));
	if (!data) {
		retval = ENOMEM;
		goto cleanup;
	}
	io->manager = test_io_manager;
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

	memset(data, 0, sizeof(struct test_private_data));
	data->magic = EXT2_ET_MAGIC_TEST_IO_CHANNEL;
	if (test_io_backing_manager) {
		retval = test_io_backing_manager->open(name, flags,
						       &data->real);
		if (retval)
			goto cleanup;
	} else
		data->real = 0;
	data->read_blk = 	test_io_cb_read_blk;
	data->write_blk = 	test_io_cb_write_blk;
	data->set_blksize = 	test_io_cb_set_blksize;
	
	*channel = io;
	return 0;

cleanup:
	if (io)
		free(io);
	if (data)
		free(data);
	return retval;
}

static errcode_t test_close(io_channel channel)
{
	struct test_private_data *data;
	errcode_t	retval = 0;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct test_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_TEST_IO_CHANNEL);

	if (--channel->refcount > 0)
		return 0;
	
	if (data->real)
		retval = io_channel_close(data->real);
	
	if (channel->private_data)
		free(channel->private_data);
	if (channel->name)
		free(channel->name);
	free(channel);
	return retval;
}

static errcode_t test_set_blksize(io_channel channel, int blksize)
{
	struct test_private_data *data;
	errcode_t	retval = 0;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct test_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_TEST_IO_CHANNEL);

	if (data->real)
		retval = io_channel_set_blksize(data->real, blksize);
	if (data->set_blksize)
		data->set_blksize(blksize, retval);
	else
		printf("Test_io: set_blksize(%d) returned %s\n",
		       blksize, retval ? error_message(retval) : "OK");
	return retval;
}


static errcode_t test_read_blk(io_channel channel, unsigned long block,
			       int count, void *buf)
{
	struct test_private_data *data;
	errcode_t	retval = 0;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct test_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_TEST_IO_CHANNEL);

	if (data->real)
		retval = io_channel_read_blk(data->real, block, count, buf);
	if (data->read_blk)
		data->read_blk(block, count, retval);
	else
		printf("Test_io: read_blk(%lu, %d) returned %s\n",
		       block, count, retval ? error_message(retval) : "OK");
	return retval;
}

static errcode_t test_write_blk(io_channel channel, unsigned long block,
			       int count, const void *buf)
{
	struct test_private_data *data;
	errcode_t	retval = 0;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct test_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_TEST_IO_CHANNEL);

	if (data->real)
		retval = io_channel_write_blk(data->real, block, count, buf);
	if (data->write_blk)
		data->write_blk(block, count, retval);
	else
		printf("Test_io: write_blk(%lu, %d) returned %s\n",
		       block, count, retval ? error_message(retval) : "OK");
	return retval;
}

/*
 * Flush data buffers to disk.
 */
static errcode_t test_flush(io_channel channel)
{
	struct test_private_data *data;
	
	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct test_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_TEST_IO_CHANNEL);

	if (data->real)
		return io_channel_flush(data->real);
	return 0;
}

