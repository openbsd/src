//
// pdisk - an editor for Apple format partition tables
//
// Written by Eryk Vershen (eryk@apple.com)
//
// Still under development (as of 15 January 1998)
//

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

// for printf()
#include <stdio.h>

#ifdef __linux__
#include <getopt.h>
#include <malloc.h>
#elif defined(__OpenBSD__)
#include <stdlib.h>
#else
// for malloc() & free()
#include <stdlib.h>
// for SIOUXsettings
#include <SIOUX.h>
#endif

// for strncpy() & strlen()
#include <string.h>
// for O_RDONLY
#include <fcntl.h>
// for errno
#include <errno.h>

#ifdef __linux__
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <linux/hdreg.h>
#endif

#include "pdisk.h"
#include "io.h"
#include "partition_map.h"
#include "pathname.h"
#include "errors.h"
#include "dump.h"
#include "validate.h"
#include "version.h"
#include "util.h"


//
// Defines
//
#define ARGV_CHUNK 5
#define CFLAG_DEFAULT	0
#define DFLAG_DEFAULT	0
#define HFLAG_DEFAULT	0
#define INTERACT_DEFAULT	0
#define LFLAG_DEFAULT	0
#define RFLAG_DEFAULT	0
#define VFLAG_DEFAULT	0


//
// Types
//


//
// Global Constants
//
enum getopt_values {
    kLongOption = 0,
    kBadOption = '?',
    kOptionArg = 1000,
    kListOption = 1001,
    kLogicalOption = 1002
};


//
// Global Variables
//
int lflag = LFLAG_DEFAULT;	/* list the device */
char *lfile;	/* list */
int vflag = VFLAG_DEFAULT;	/* show version */
int hflag = HFLAG_DEFAULT;	/* show help */
int dflag = DFLAG_DEFAULT;	/* turn on debugging commands and printout */
int rflag = RFLAG_DEFAULT;	/* open device read Only */
int interactive = INTERACT_DEFAULT;
int cflag = CFLAG_DEFAULT;	/* compute device size */

static int first_get = 1;


//
// Forward declarations
//
void do_change_map_size(partition_map_header *map);
void do_create_partition(partition_map_header *map, int get_type);
void do_delete_partition(partition_map_header *map);
void do_display_block(partition_map_header *map, char *alt_name);
void do_display_entry(partition_map_header *map);
void do_examine_patch_partition(partition_map_header *map);
int do_expert(partition_map_header *map, char *name);
void do_rename_partition(partition_map_header *map);
void do_change_type(partition_map_header *map);
void do_reorder(partition_map_header *map);
void do_write_partition_map(partition_map_header *map);
void edit(char *name, int ask_logical_size);
int get_base_argument(long *number, partition_map_header *map);
int get_command_line(int *argc, char ***argv);
int get_size_argument(long *number, partition_map_header *map);
int get_options(int argc, char **argv);
void interact();
void print_edit_notes();
void print_expert_notes();
void print_top_notes();


//
// Routines
//
int
main(int argc, char **argv)
{
#if defined(__linux__) || defined (__OpenBSD__)
    int name_index;
#else
    printf("This app uses the SIOUX console library\n");
    printf("Choose 'Quit' from the file menu to quit.\n\n");
    printf("Use fake disk names (/dev/scsi<bus>.<id>; i.e. /dev/scsi0.1, /dev/scsi1.3, etc.).\n\n");

    SIOUXSettings.autocloseonquit = 0;	/* Do we close the SIOUX window on program termination ... */
    SIOUXSettings.asktosaveonclose = 0;	/* Do we offer to save on a close ... */
#endif
    char *versionstr;

    init_program_name(argv);

    if (sizeof(DPME) != PBLOCK_SIZE) {
	fatal(-1, "Size of partion map entry (%d) "
		"is not equal to block size (%d)\n",
		sizeof(DPME), PBLOCK_SIZE);
    }
    if (sizeof(Block0) != PBLOCK_SIZE) {
	fatal(-1, "Size of block zero structure (%d) "
		"is not equal to block size (%d)\n",
		sizeof(Block0), PBLOCK_SIZE);
    }
    versionstr = (char *)get_version_string();
    if (versionstr) {
    	if (strcmp(VERSION, versionstr) != 0) {
		fatal(-1, "Version string static form (%s) does not match dynamic form (%s)\n",
		    VERSION, versionstr);
    	}
	free(versionstr);
    }

#if defined(__linux__) || defined(__OpenBSD__)
    name_index = get_options(argc, argv);

    if (vflag) {
	printf("version " VERSION " (" RELEASE_DATE ")\n");
    }
    if (hflag) {
 	do_help();
    } else if (interactive) {
	interact();
    } else if (lflag) {
	if (lfile != NULL) {
	    dump(lfile);
	} else if (name_index < argc) {
	    while (name_index < argc) {
		dump(argv[name_index++]);
	    }
	} else {
	    list_all_disks();
	}
    } else if (name_index < argc) {
	while (name_index < argc) {
	    edit(argv[name_index++], 0);
	}
    } else if (!vflag) {
	usage("no device argument");
 	do_help();
    }
    return 0;
#else
    interactive = 1;

    interact();

    SIOUXSettings.autocloseonquit = 1;
    //printf("Processing stopped: Choose 'Quit' from the file menu to quit.\n\n");
    return (0);
#endif
}


void
print_top_notes()
{
    printf("Notes:\n");
    printf("  Disks have fake names of the form /dev/scsi<bus>.<id>\n");
    printf("  For example, /dev/scsi0.1, /dev/scsi1.3, and so on.\n");
    printf("  MkLinux style names are also allowed (i.e /dev/sda or /dev/hda),\n");
    printf("  and these names follow the MkLinux DR3 conventions.\n");
    printf("\n");
}


void
interact()
{
    char *name;
    int command;
    int ask_logical_size;

    while (get_command("Top level command (? for help): ", first_get, &command)) {
	first_get = 0;
	ask_logical_size = 0;

	switch (command) {
	case '?':
	    print_top_notes();
	    // fall through
	case 'H':
	case 'h':
	    printf("Commands are:\n");
	    printf("  h    print help\n");
	    printf("  v    print the version number and release date\n");
	    printf("  l    list device's map\n");
	    printf("  L    list all devices' maps\n");
	    printf("  e    edit device's map\n");
	    printf("  E    (edit map with specified block size)\n");
	    printf("  r    toggle readonly flag\n");
	    if (dflag) {
		printf("  a    toggle abbreviate flag\n");
		printf("  p    toggle physical flag\n");
		printf("  c    toggle compute size flag\n");
		printf("  d    toggle debug flag\n");
		printf("  x    examine block n of device\n");
	    }
	    printf("  q    quit the program\n");
	    break;
	case 'Q':
	case 'q':
	    return;
	    break;
	case 'V':
	case 'v':
	    printf("version " VERSION " (" RELEASE_DATE ")\n");
	    break;
	case 'L':
	    list_all_disks();
	    break;
	case 'l':
	    if (get_string_argument("Name of device: ", &name, 1) == 0) {
		bad_input("Bad name");
		break;
	    }
	    dump(name);
	    free(name);
	    break;
	case 'E':
	    ask_logical_size = 1;
	case 'e':
	    if (get_string_argument("Name of device: ", &name, 1) == 0) {
		bad_input("Bad name");
		break;
	    }
	    edit(name, ask_logical_size);
	    free(name);
	    break;
	case 'R':
	case 'r':
	    if (rflag) {
		rflag = 0;
	    } else {
		rflag = 1;
	    }
	    printf("Now in %s mode.\n", (rflag)?"readonly":"read/write");
	    break;
	case 'A':
	case 'a':
	    if (dflag) {
		if (aflag) {
		    aflag = 0;
		} else {
		    aflag = 1;
		}
		printf("Now in %s mode.\n", (aflag)?"abbreviate":"full type");
	    } else {
	    	goto do_error;
	    }
	    break;
	case 'P':
	case 'p':
	    if (dflag) {
		if (pflag) {
		    pflag = 0;
		} else {
		    pflag = 1;
		}
		printf("Now in %s mode.\n", (pflag)?"physical":"logical");
	    } else {
	    	goto do_error;
	    }
	    break;
	case 'D':
	case 'd':
	    if (dflag) {
		dflag = 0;
	    } else {
		dflag = 1;
	    }
	    printf("Now in %s mode.\n", (dflag)?"debug":"normal");
	    break;
	case 'C':
	case 'c':
	    if (dflag) {
		if (cflag) {
		    cflag = 0;
		} else {
		    cflag = 1;
		}
		printf("Now in %s device size mode.\n", (cflag)?"always compute":"use existing");
	    } else {
	    	goto do_error;
	    }
	    break;
	case 'X':
	case 'x':
	    if (dflag) {
		do_display_block(0, 0);
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
}


#ifdef __linux__
int
get_options(int argc, char **argv)
{
    int c;
    static struct option long_options[] =
    {
	// name		has_arg			&flag	val
	{"help",	no_argument,		0,	'h'},
	{"list",	optional_argument,	0,	kListOption},
	{"version",	no_argument,		0,	'v'},
	{"debug",	no_argument,		0,	'd'},
	{"readonly",	no_argument,		0,	'r'},
	{"abbr",	no_argument,		0,	'a'},
	{"logical",	no_argument,		0,	kLogicalOption},
	{"interactive",	no_argument,		0,	'i'},
	{"compute_size", no_argument,		0,	'c'},
	{0, 0, 0, 0}
    };
    int option_index = 0;
    extern int optind;
    extern char *optarg;
    int flag = 0;

    lflag = LFLAG_DEFAULT;
    lfile = NULL;
    vflag = VFLAG_DEFAULT;
    hflag = HFLAG_DEFAULT;
    dflag = DFLAG_DEFAULT;
    rflag = RFLAG_DEFAULT;
    aflag = AFLAG_DEFAULT;
    pflag = PFLAG_DEFAULT;
    interactive = INTERACT_DEFAULT;
    cflag = CFLAG_DEFAULT;

    optind = 0;	// reset option scanner logic
    while ((c = getopt_long(argc, argv, "hlvdric", long_options,
	    &option_index)) >= 0) {
	switch (c) {
	case kLongOption:
	    // option_index would be used here
	    break;
	case 'h':
	    hflag = (HFLAG_DEFAULT)?0:1;
	    break;
	case kListOption:
	    if (optarg != NULL) {
		lfile = optarg;
	    }
	    // fall through
	case 'l':
	    lflag = (LFLAG_DEFAULT)?0:1;
	    break;
	case 'v':
	    vflag = (VFLAG_DEFAULT)?0:1;
	    break;
	case 'd':
	    dflag = (DFLAG_DEFAULT)?0:1;
	    break;
	case 'c':
	    cflag = (CFLAG_DEFAULT)?0:1;
	    break;
	case 'r':
	    rflag = (RFLAG_DEFAULT)?0:1;
	    break;
	case 'i':
	    interactive = (INTERACT_DEFAULT)?0:1;
	    break;
	case 'a':
	    aflag = (AFLAG_DEFAULT)?0:1;
	    break;
	case kLogicalOption:
	    pflag = (PFLAG_DEFAULT)?0:1;
	    break;
	case kBadOption:
	default:
	    flag = 1;
	    break;
	}
    }
    if (flag) {
	usage("bad arguments");
    }
    return optind;
}
#endif

#ifdef __OpenBSD__
int
get_options(int argc, char **argv)
{
    int c;
    extern int optind;
    extern char *optarg;
    int flag = 0;

    lflag = LFLAG_DEFAULT;
    lfile = NULL;
    vflag = VFLAG_DEFAULT;
    hflag = HFLAG_DEFAULT;
    dflag = DFLAG_DEFAULT;
    rflag = RFLAG_DEFAULT;
    aflag = AFLAG_DEFAULT;
    pflag = PFLAG_DEFAULT;
    interactive = INTERACT_DEFAULT;
    cflag = CFLAG_DEFAULT;

    optind = 1;	// reset option scanner logic
    while ((c = getopt(argc, argv, "hlvdric")) != -1) {
	switch (c) {
	case 'h':
	    hflag = (HFLAG_DEFAULT)?0:1;
	    break;
	case 'l':
	    lflag = (LFLAG_DEFAULT)?0:1;
	    break;
	case 'v':
	    vflag = (VFLAG_DEFAULT)?0:1;
	    break;
	case 'd':
	    dflag = (DFLAG_DEFAULT)?0:1;
	    break;
	case 'c':
	    cflag = (CFLAG_DEFAULT)?0:1;
	    break;
	case 'r':
	    rflag = (RFLAG_DEFAULT)?0:1;
	    break;
	case 'i':
	    interactive = (INTERACT_DEFAULT)?0:1;
	    break;
	case 'a':
	    aflag = (AFLAG_DEFAULT)?0:1;
	    break;
	case kLogicalOption:
	    pflag = (PFLAG_DEFAULT)?0:1;
	    break;
	default:
	    flag = 1;
	    break;
	}
    }
    if (flag) {
	usage("bad arguments");
    }
    return optind;
}
#endif


void
print_edit_notes()
{
    printf("Notes:\n");
    printf("  Base and length fields are blocks, which vary in size between media.\n");
    printf("  The base field can be <nth>p; i.e. use the base of the nth partition.\n");
    printf("  The length field can be a length followed by k, m, g or t to indicate\n");
    printf("  kilo, mega, giga, or tera bytes; also the length can be <nth>p; i.e. use\n");
    printf("  the length of the nth partition.\n");
    printf("  The name of a partition is descriptive text.\n");
    printf("\n");
}


//
// Edit the file
//
void
edit(char *name, int ask_logical_size)
{
    partition_map_header *map;
    int command;
    int order;
    int get_type;
    int valid_file;

    map = open_partition_map(name, &valid_file, ask_logical_size);
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
	    // fall through
	case 'H':
	case 'h':
	    printf("Commands are:\n");
	    printf("  h    help\n");
	    printf("  p    print the partition table\n");
	    printf("  P    (print ordered by base address)\n");
	    printf("  i    initialize partition map\n");
	    printf("  s    change size of partition map\n");
	    printf("  c    create new partition (standard OpenBSD type)\n");
	    printf("  C    (create with type also specified)\n");
	    printf("  n    (re)name a partition\n");
	    printf("  d    delete a partition\n");
	    printf("  r    reorder partition entry in map\n");
	    printf("  t    change a partition's type\n");
	    if (!rflag) {
		printf("  w    write the partition table\n");
	    }
	    printf("  q    quit editing (don't save changes)\n");
	    if (dflag) {
		printf("  x    extra extensions for experts\n");
	    }
	    break;
	case 'P':
	    order = 0;
	    // fall through
	case 'p':
	    dump_partition_map(map, order);
	    break;
	case 'Q':
	case 'q':
	    flush_to_newline(1);
	    goto finis;
	    break;
	case 'I':
	case 'i':
	    map = init_partition_map(name, map);
	    break;
	case 'C':
	    get_type = 1;
	    // fall through
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
do_create_partition(partition_map_header *map, int get_type)
{
    long base;
    long length;
    char *name;
    char *type_name;

    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }
    if (!rflag && map->writeable == 0) {
	printf("The map is not writable.\n");
    }
// XXX add help feature (i.e. '?' in any argument routine prints help string)
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
	goto xit1;

    } else if (get_string_argument("Type of partition: ", &type_name, 1) == 0) {
	bad_input("Bad type");
	goto xit1;
    } else {
	if (istrncmp(type_name, kFreeType, DPISTRLEN) == 0) {
	    bad_input("Can't create a partition with the Free type");
	    goto xit2;
	}
	if (istrncmp(type_name, kMapType, DPISTRLEN) == 0) {
	    bad_input("Can't create a partition with the Map type");
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
get_base_argument(long *number, partition_map_header *map)
{
    partition_map * entry;
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
get_size_argument(long *number, partition_map_header *map)
{
    partition_map * entry;
    int result = 0;
    unsigned long multiple;

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
do_rename_partition(partition_map_header *map)
{
    partition_map * entry;
    long index;
    char *name;

    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }
    if (!rflag && map->writeable == 0) {
	printf("The map is not writable.\n");
    }
    if (get_number_argument("Partition number: ", &index, kDefault) == 0) {
	bad_input("Bad partition number");
	return;
    }
    if (get_string_argument("New name of partition: ", &name, 1) == 0) {
	bad_input("Bad name");
	return;
    }

	// find partition and change it
    entry = find_entry_by_disk_address(index, map);
    if (entry == NULL) {
	printf("No such partition\n");
    } else {
	// stuff name into partition map entry data
	strncpy(entry->data->dpme_name, name, DPISTRLEN);
	map->changed = 1;
    }
    free(name);
    return;
}

void
do_change_type(partition_map_header *map)
{
    partition_map * entry;
    long index;
    char *type = NULL;

    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }

    if (!rflag && map->writeable == 0) {
	printf("The map is not writable.\n");
    }

    if (get_number_argument("Partition number: ", &index, kDefault) == 0) {
	bad_input("Bad partition number");
	return;
    }

    entry = find_entry_by_disk_address(index, map);

    if (entry == NULL ) {
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
    if (type)
        free(type);
    return;
}


void
do_delete_partition(partition_map_header *map)
{
    partition_map * cur;
    long index;

    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }
    if (!rflag && map->writeable == 0) {
	printf("The map is not writable.\n");
    }
    if (get_number_argument("Partition number: ", &index, kDefault) == 0) {
	bad_input("Bad partition number");
	return;
    }

	// find partition and delete it
    cur = find_entry_by_disk_address(index, map);
    if (cur == NULL) {
	printf("No such partition\n");
    } else {
	delete_partition_from_map(cur);
    }
}


void
do_reorder(partition_map_header *map)
{
    long old_index;
    long index;

    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }
    if (!rflag && map->writeable == 0) {
	printf("The map is not writable.\n");
    }
    if (get_number_argument("Partition number: ", &old_index, kDefault) == 0) {
	bad_input("Bad partition number");
	return;
    }
    if (get_number_argument("New number: ", &index, kDefault) == 0) {
	bad_input("Bad partition number");
	return;
    }

    move_entry_in_map(old_index, index, map);
}


void
do_write_partition_map(partition_map_header *map)
{
    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }
    if (map->changed == 0) {
	bad_input("The map has not been changed.");
	return;
    }
    if (map->writeable == 0) {
	bad_input("The map is not writable.");
	return;
    }
    printf("Writing the map destroys what was there before. ");
    if (get_okay("Is that okay? [n/y]: ", 0) != 1) {
	return;
    }

    write_partition_map(map);

    // exit(0);
}


void
print_expert_notes()
{
    printf("Notes:\n");
    printf("  The expert commands are for low level and experimental features.\n");
    printf("  These commands are available only when debug mode is on.\n");
    printf("\n");
}


int
do_expert(partition_map_header *map, char *name)
{
    int command;
    int quit = 0;

    while (get_command("Expert command (? for help): ", first_get, &command)) {
	first_get = 0;

	switch (command) {
	case '?':
	    print_expert_notes();
	    // fall through
	case 'H':
	case 'h':
	    printf("Commands are:\n");
	    printf("  h    print help\n");
	    printf("  d    dump block n\n");
	    printf("  p    print the partition table\n");
	    if (dflag) {
		printf("  P    (show data structures  - debugging)\n");
	    }
	    printf("  f    full display of nth entry\n");
	    printf("  v    validate map\n");
	    printf("  e    examine patch partition\n");
	    printf("  q    return to main edit menu\n");
	    printf("  Q    quit without saving changes\n");
	    break;
	case 'q':
	    flush_to_newline(1);
	    goto finis;
	    break;
	case 'Q':
	    quit = 1;
	    goto finis;
	    break;
	case 'P':
	    if (dflag) {
		show_data_structures(map);
		break;
	    }
	    // fall through
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
	case 'E':
	case 'e':
	    do_examine_patch_partition(map);
	    break;
	default:
#ifndef __OpenBSD__
	do_error:
#endif
	    bad_input("No such command (%c)", command);
	    break;
	}
    }
finis:
    return quit;
}

void
do_change_map_size(partition_map_header *map)
{
    long size;

    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }
    if (!rflag && map->writeable == 0) {
	printf("The map is not writable.\n");
    }
    if (get_number_argument("New size: ", &size, kDefault) == 0) {
	bad_input("Bad size");
	return;
    }
    resize_map(size, map);
}


void
do_display_block(partition_map_header *map, char *alt_name)
{
    MEDIA m;
    long number;
    char *name;
    static unsigned char *display_block;
    static int display_g;
    int g;

    if (map != NULL) {
    	name = 0;
	m = map->m;
	g = map->logical_block;
    } else {
	if (alt_name == 0) {
	    if (get_string_argument("Name of device: ", &name, 1) == 0) {
		bad_input("Bad name");
		return;
	    }
	} else {
	    name = strdup(alt_name);
	}
	m = open_pathname_as_media(name, O_RDONLY);
	if (m == 0) {
	    error(errno, "can't open file '%s'", name);
	    free(name);
	    return;
	}
	g = media_granularity(m);
	if (g < PBLOCK_SIZE) {
	    g = PBLOCK_SIZE;
	}
    }
    if (get_number_argument("Block number: ", &number, kDefault) == 0) {
	bad_input("Bad block number");
	goto xit;
    }
    if (display_block == NULL || display_g < g) {
    	if (display_block != NULL) {
    	    free(display_block);
    	    display_g = 0;
	}
	display_block = (unsigned char *) malloc(g);
	if (display_block == NULL) {
	    error(errno, "can't allocate memory for display block buffer");
	    goto xit;
	}
	display_g = g;
    }
    if (read_media(m, ((long long)number) * g, g, (char *)display_block) != 0) {
	dump_block((unsigned char*) display_block, g);
    }

xit:
    if (name) {
	close_media(m);
	free(name);
    }
    return;
}


void
do_display_entry(partition_map_header *map)
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


void
do_examine_patch_partition(partition_map_header *map)
{
    partition_map * entry;

    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }
    entry = find_entry_by_type(kPatchType, map);
    if (entry == NULL) {
	printf("No patch partition\n");
    } else {
	display_patches(entry);
    }
}
