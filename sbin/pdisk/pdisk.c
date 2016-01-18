/*	$OpenBSD: pdisk.c,v 1.49 2016/01/18 21:50:53 krw Exp $	*/

/*
 * pdisk - an editor for Apple format partition tables
 *
 * Written by Eryk Vershen
 *
 * Still under development (as of 15 January 1998)
 */

/*
 * Copyright 1996,1997,1998 by Apple Computer, Inc.
 *              All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>		/* DEV_BSIZE */
#include <err.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "io.h"
#include "partition_map.h"
#include "dump.h"
#include "validate.h"
#include "file_media.h"

int		lflag;		/* list the device */
int		dflag;		/* turn on debugging commands and printout */
int		rflag;		/* open device read Only */

static int	first_get = 1;

void		do_change_map_size(struct partition_map_header *);
void		do_create_partition(struct partition_map_header *, int);
void		do_delete_partition(struct partition_map_header *);
void		do_display_block(struct partition_map_header *, char *);
void		do_display_entry(struct partition_map_header *);
int		do_expert (struct partition_map_header *, char *);
void		do_rename_partition(struct partition_map_header *);
void		do_change_type(struct partition_map_header *);
void		do_reorder(struct partition_map_header *);
void		do_write_partition_map(struct partition_map_header *);
void		edit(char *);
int		get_base_argument(long *, struct partition_map_header *);
int		get_command_line(int *, char ***);
int		get_size_argument(long *, struct partition_map_header *);
void		print_edit_notes(void);
void		print_expert_notes(void);

__dead static void usage(void);

int
main(int argc, char **argv)
{
	int c, name_index;

	if (sizeof(struct dpme) != DEV_BSIZE) {
		errx(1, "Size of partition map entry (%zu) is not equal "
		    "to block size (%d)\n", sizeof(struct dpme), DEV_BSIZE);
	}
	if (sizeof(struct block0) != DEV_BSIZE) {
		errx(1, "Size of block zero structure (%zu) is not equal "
		    "to block size (%d)\n", sizeof(struct block0), DEV_BSIZE);
	}
	while ((c = getopt(argc, argv, "ldr")) != -1) {
		switch (c) {
		case 'l':
			lflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		default:
			usage();
			break;
		}
	}

	name_index = optind;

	if (lflag) {
		if (name_index < argc) {
			while (name_index < argc) {
				dump(argv[name_index++]);
			}
		} else {
			usage();
		}
	} else if (name_index < argc) {
		while (name_index < argc) {
			edit(argv[name_index++]);
		}
	} else {
		usage();
	}
	return 0;
}

void
print_edit_notes()
{
	printf("Notes:\n");
	printf("  Base and length fields are blocks, which vary in size "
	    "between media.\n");
	printf("  The base field can be <nth>p; i.e. use the base of the "
	    "nth partition.\n");
	printf("  The length field can be a length followed by k, m, g or "
	    "t to indicate\n");
	printf("  kilo, mega, giga, or tera bytes; also the length can be "
	    "<nth>p; i.e. use\n");
	printf("  the length of the nth partition.\n");
	printf("  The name of a partition is descriptive text.\n");
	printf("\n");
}

/*
 * Edit the file
 */
void
edit(char *name)
{
	struct partition_map_header *map;
	int command, order, get_type, valid_file;

	map = open_partition_map(name, &valid_file);
	if (!valid_file) {
		return;
	}
	printf("Edit %s -\n", name);

	while (get_command("Command (? for help): ", first_get, &command)) {
		first_get = 0;
		order = 1;
		get_type = 0;

		switch (command) {
		case '?':
			print_edit_notes();
			/* fall through */
		case 'H':
		case 'h':
			printf("Commands are:\n");
			printf("  C    (create with type also specified)\n");
			printf("  c    create new partition (standard OpenBSD "
			    "root)\n");
			printf("  d    delete a partition\n");
			printf("  h    help\n");
			printf("  i    initialize partition map\n");
			printf("  n    (re)name a partition\n");
			printf("  P    (print ordered by base address)\n");
			printf("  p    print the partition table\n");
			printf("  q    quit editing\n");
			printf("  r    reorder partition entry in map\n");
			printf("  s    change size of partition map\n");
			printf("  t    change a partition's type\n");
			if (!rflag) {
				printf("  w    write the partition table\n");
			}
			if (dflag) {
				printf("  x    extra extensions for experts\n");
			}
			break;
		case 'P':
			order = 0;
			/* fall through */
		case 'p':
			dump_partition_map(map, order);
			break;
		case 'Q':
		case 'q':
			if (map && map->changed) {
				if (get_okay("Discard changes? [n/y]: ", 0) !=
				    1) {
					break;
				}
			}
			flush_to_newline(1);
			goto finis;
			break;
		case 'I':
		case 'i':
			map = init_partition_map(name, map);
			break;
		case 'C':
			get_type = 1;
			/* fall through */
		case 'c':
			do_create_partition(map, get_type);
			break;
		case 'N':
		case 'n':
			do_rename_partition(map);
			break;
		case 'D':
		case 'd':
			do_delete_partition(map);
			break;
		case 'R':
		case 'r':
			do_reorder(map);
			break;
		case 'S':
		case 's':
			do_change_map_size(map);
			break;
		case 'T':
		case 't':
			do_change_type(map);
			break;
		case 'X':
		case 'x':
			if (!dflag) {
				goto do_error;
			} else if (do_expert(map, name)) {
				flush_to_newline(1);
				goto finis;
			}
			break;
		case 'W':
		case 'w':
			if (!rflag) {
				do_write_partition_map(map);
			} else {
				goto do_error;
			}
			break;
		default:
	do_error:
			bad_input("No such command (%c)", command);
			break;
		}
	}
finis:

	close_partition_map(map);
}

void
do_create_partition(struct partition_map_header * map, int get_type)
{
	long base, length;
	char *name = NULL;
	char *type_name = NULL;

	if (map == NULL) {
		bad_input("No partition map exists");
		return;
	}
	if (!rflag && map->writable == 0) {
		printf("The map is not writable.\n");
	}
	if (get_base_argument(&base, map) == 0) {
		return;
	}
	if (get_size_argument(&length, map) == 0) {
		return;
	}
	if (get_string_argument("Name of partition: ", &name, 1) == 0) {
		bad_input("Bad name");
		return;
	}
	if (get_type == 0) {
		add_partition_to_map(name, kUnixType, base, length, map);
	} else if (get_string_argument("Type of partition: ", &type_name, 1) ==
	    0) {
		bad_input("Bad type");
		goto xit1;
	} else {
		if (strncasecmp(type_name, kFreeType, DPISTRLEN) == 0) {
			bad_input("Can't create a partition with the Free "
			    "type");
			goto xit2;
		}
		if (strncasecmp(type_name, kMapType, DPISTRLEN) == 0) {
			bad_input("Can't create a partition with the Map "
			    "type");
			goto xit2;
		}
		add_partition_to_map(name, type_name, base, length, map);
	}
xit2:
	free(type_name);
xit1:
	free(name);
	return;
}

int
get_base_argument(long *number, struct partition_map_header * map)
{
	struct partition_map *entry;
	int result = 0;

	if (get_number_argument("First block: ", number, kDefault) == 0) {
		bad_input("Bad block number");
	} else {
		result = 1;
		if (get_partition_modifier()) {
			entry = find_entry_by_disk_address(*number, map);
			if (entry == NULL) {
				bad_input("Bad partition number");
				result = 0;
			} else {
				*number = entry->data->dpme_pblock_start;
			}
		}
	}
	return result;
}


int
get_size_argument(long *number, struct partition_map_header * map)
{
	struct partition_map *entry;
	unsigned long multiple;
	int result = 0;

	if (get_number_argument("Length in blocks: ", number, kDefault) == 0) {
		bad_input("Bad length");
	} else {
		multiple = get_multiplier(map->logical_block);
		if (multiple == 0) {
			bad_input("Bad multiplier");
		} else if (multiple != 1) {
			*number *= multiple;
			result = 1;
		} else if (get_partition_modifier()) {
			entry = find_entry_by_disk_address(*number, map);
			if (entry == NULL) {
				bad_input("Bad partition number");
			} else {
				*number = entry->data->dpme_pblocks;
				result = 1;
			}
		} else {
			result = 1;
		}
	}
	return result;
}


void
do_rename_partition(struct partition_map_header * map)
{
	struct partition_map *entry;
	char *name;
	long ix;

	if (map == NULL) {
		bad_input("No partition map exists");
		return;
	}
	if (!rflag && map->writable == 0) {
		printf("The map is not writable.\n");
	}
	if (get_number_argument("Partition number: ", &ix, kDefault) == 0) {
		bad_input("Bad partition number");
		return;
	}
	if (get_string_argument("New name of partition: ", &name, 1) == 0) {
		bad_input("Bad name");
		return;
	}
	/* find partition and change it */
	entry = find_entry_by_disk_address(ix, map);
	if (entry == NULL) {
		printf("No such partition\n");
	} else {
		/* stuff name into partition map entry data */
		strncpy(entry->data->dpme_name, name, DPISTRLEN);
		map->changed = 1;
	}
	free(name);
	return;
}

void
do_change_type(struct partition_map_header * map)
{
	struct partition_map *entry;
	char *type = NULL;
	long ix;

	if (map == NULL) {
		bad_input("No partition map exists");
		return;
	}
	if (!rflag && map->writable == 0) {
		printf("The map is not writable.\n");
	}
	if (get_number_argument("Partition number: ", &ix, kDefault) == 0) {
		bad_input("Bad partition number");
		return;
	}
	entry = find_entry_by_disk_address(ix, map);

	if (entry == NULL) {
		printf("No such partition\n");
		goto out;
	}
	printf("Existing partition type ``%s''.\n", entry->data->dpme_type);
	if (get_string_argument("New type of partition: ", &type, 1) == 0) {
		bad_input("Bad type");
		goto out;
	}
	strncpy(entry->data->dpme_type, type, DPISTRLEN);
	map->changed = 1;

out:
	free(type);
	return;
}


void
do_delete_partition(struct partition_map_header * map)
{
	struct partition_map *cur;
	long ix;

	if (map == NULL) {
		bad_input("No partition map exists");
		return;
	}
	if (!rflag && map->writable == 0) {
		printf("The map is not writable.\n");
	}
	if (get_number_argument("Partition number: ", &ix, kDefault) == 0) {
		bad_input("Bad partition number");
		return;
	}
	/* find partition and delete it */
	cur = find_entry_by_disk_address(ix, map);
	if (cur == NULL) {
		printf("No such partition\n");
	} else {
		delete_partition_from_map(cur);
	}
}


void
do_reorder(struct partition_map_header * map)
{
	long ix, old_index;

	if (map == NULL) {
		bad_input("No partition map exists");
		return;
	}
	if (!rflag && map->writable == 0) {
		printf("The map is not writable.\n");
	}
	if (get_number_argument("Partition number: ", &old_index, kDefault) ==
	    0) {
		bad_input("Bad partition number");
		return;
	}
	if (get_number_argument("New number: ", &ix, kDefault) == 0) {
		bad_input("Bad partition number");
		return;
	}
	move_entry_in_map(old_index, ix, map);
}


void
do_write_partition_map(struct partition_map_header * map)
{
	if (map == NULL) {
		bad_input("No partition map exists");
		return;
	}
	if (map->changed == 0 && map->written == 0) {
		bad_input("The map has not been changed.");
		return;
	}
	if (map->writable == 0) {
		bad_input("The map is not writable.");
		return;
	}
	printf("Writing the map destroys what was there before. ");
	if (get_okay("Is that okay? [n/y]: ", 0) != 1) {
		return;
	}
	write_partition_map(map);

	map->changed = 0;
	map->written = 1;
}


void
print_expert_notes()
{
	printf("Notes:\n");
	printf("  The expert commands are for low level and experimental "
	    "features.\n");
	printf("  These commands are available only when debug mode is on.\n");
	printf("\n");
}


int
do_expert(struct partition_map_header * map, char *name)
{
	int command, quit = 0;

	while (get_command("Expert command (? for help): ", first_get,
		    &command)) {
		first_get = 0;

		switch (command) {
		case '?':
			print_expert_notes();
			/* fall through */
		case 'H':
		case 'h':
			printf("Commands are:\n");
			printf("  h    print help\n");
			printf("  d    dump block n\n");
			printf("  p    print the partition table\n");
			if (dflag) {
				printf("  P    (show data structures  - "
				    "debugging)\n");
			}
			printf("  f    full display of nth entry\n");
			printf("  v    validate map\n");
			printf("  q    return to main edit menu\n");
			printf("  Q    quit editing\n");
			break;
		case 'q':
			flush_to_newline(1);
			goto finis;
			break;
		case 'Q':
			if (map->changed) {
				if (get_okay("Discard changes? [n/y]: ", 0) !=
				    1) {
					break;
				}
			}
			quit = 1;
			goto finis;
			break;
		case 'P':
			if (dflag) {
				show_data_structures(map);
				break;
			}
			/* fall through */
		case 'p':
			dump_partition_map(map, 1);
			break;
		case 'D':
		case 'd':
			do_display_block(map, name);
			break;
		case 'F':
		case 'f':
			do_display_entry(map);
			break;
		case 'V':
		case 'v':
			validate_map(map);
			break;
		default:
			bad_input("No such command (%c)", command);
			break;
		}
	}
finis:
	return quit;
}

void
do_change_map_size(struct partition_map_header * map)
{
	long size;

	if (map == NULL) {
		bad_input("No partition map exists");
		return;
	}
	if (!rflag && map->writable == 0) {
		printf("The map is not writable.\n");
	}
	if (get_number_argument("New size: ", &size, kDefault) == 0) {
		bad_input("Bad size");
		return;
	}
	resize_map(size, map);
}


void
do_display_block(struct partition_map_header * map, char *alt_name)
{
	char *name;
	long number;
	int fd, g;
	static unsigned char *display_block;
	static long next_number = -1;
	static int display_g;

	if (map != NULL) {
		name = 0;
		fd = map->fd;
		g = map->logical_block;
	} else {
		if (alt_name == 0) {
			if (get_string_argument("Name of device: ", &name, 1)
			    == 0) {
				bad_input("Bad name");
				return;
			}
		} else {
			if ((name = strdup(alt_name)) == NULL) {
				warn("strdup failed");
				return;
			}
		}
		fd = open_file_as_media(name, O_RDONLY);
		if (fd == -1) {
			warn("can't open file '%s'", name);
			free(name);
			return;
		}
		g = DEV_BSIZE;
	}
	if (get_number_argument("Block number: ", &number, next_number) == 0) {
		bad_input("Bad block number");
		goto xit;
	}
	if (display_block == NULL || display_g < g) {
		if (display_block != NULL) {
			free(display_block);
			display_g = 0;
		}
		display_block = malloc(g);
		if (display_block == NULL) {
			warn("can't allocate memory for display block buffer");
			goto xit;
		}
		display_g = g;
	}
	if (read_file_media(fd, ((long long) number) * g, g,
		    (char *)display_block) != 0) {
		printf("block %ld -", number);
		dump_block((unsigned char *)display_block, g);
		next_number = number + 1;
	}
xit:
	if (name) {
		close(fd);
		free(name);
	}
	return;
}


void
do_display_entry(struct partition_map_header * map)
{
	long number;

	if (map == NULL) {
		bad_input("No partition map exists");
		return;
	}
	if (get_number_argument("Partition number: ", &number, kDefault) == 0) {
		bad_input("Bad partition number");
		return;
	}
	if (number == 0) {
		full_dump_block_zero(map);
	} else {
		full_dump_partition_entry(map, number);
	}
}


__dead static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-lr] disk\n", __progname);

	exit(1);
}
