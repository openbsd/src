/* -------------------------------------------------- 
 |  NAME
 |    streamtodev
 |  PURPOSE
 |    dump all data from stream to a device.
 |   
 |  NOTES
 |    only works for RDB partitions.
 |
 |  COPYRIGHT
 |    Copyright (C) 1993  Christian E. Hopps
 |
 |    This program is free software; you can redistribute it and/or modify
 |    it under the terms of the GNU General Public License as published by
 |    the Free Software Foundation; either version 2 of the License, or
 |    (at your option) any later version.
 |
 |    This program is distributed in the hope that it will be useful,
 |    but WITHOUT ANY WARRANTY; without even the implied warranty of
 |    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 |    GNU General Public License for more details.
 |
 |    You should have received a copy of the GNU General Public License
 |    along with this program; if not, write to the Free Software
 |    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 |
 |  HISTORY
 |    chopps - Oct 9, 1993: Created.
 +--------------------------------------------------- */

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/rdargs.h>
#include <cstartup.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#define __GNU_LIBRARY__ 1
#include <getopt.h>
#undef __GNU_LIBRARY__
#include "getdevices.h"

#if defined (SASC)
/* we will handle this ourselves. */
int __regargs chkabort (void)
{
    return 0;
}
int __regargs Chk_Abort (void)
{
    return 0;
}
#endif

struct partition *find_partition (struct List *dl, char *dev_name, char *part_name,
				  ulong unit, ulong start_block, ulong end_block);
void file_to_dev (char *name, ulong unit, ulong bpb, FILE *file, ulong cb, ulong end);
int check_values (struct partition *p, ulong st, ulong end, int exp);

struct option long_options[] = {
    { "input", required_argument, NULL, 'f' },
    { "rdb-name", required_argument, NULL, 'n'},
#if defined (EXPERT_VERSION)
    { "start-block", required_argument, NULL, 's'},
    { "end-block", required_argument, NULL, 'e'},
    { "expert-mode", no_argument, NULL, 'x'},
#endif
    { "device", required_argument, NULL, 'd'},
    { "unit", required_argument, NULL, 'u'},
    { "buffer-blocks", required_argument, NULL, 'b'},
    { "verbose", no_argument, NULL, 'V'},
    { "debug-mode", no_argument, NULL, 'g'},
    { "help", no_argument, NULL, 'h'},
    { "version", no_argument, NULL, 'v' },
    { "quiet", no_argument, NULL, 'q' },
    { NULL, 0, NULL, 0 }
};
char *short_options = "?qvVghn:d:u:b:f:"
#if defined (EXPERT_VERSION)
"xs:e:"
#endif
;

char *cmd_vers_string = "\0$VERS devtostream 1.0 (93.10.10)";
char *version_string = "devtostream V1.0 -- Copyright 1993 Christian E. Hopps\n";

char *help_string = "Usage: %s [options]\n"
"Options:\n"
"    -[vVxghnsedubo] [--input=file] [--rdb-name=partition_name]\n"
#if defined (EXPERT_VERSION)
"    [--start-block=block] [--end-block=block] [--expert-mode]\n"
#endif
"    [--device=device_name] [--unit=unit_num]  [--version]\n"
"    [--buffer-blocks=blocks] [--verbose] [--quiet] [--help]\n"
"\n"
"Number Formats: (where `n\' are alpha-num. digits)\n"
"    0[xX]nnn | [xX]nnn | nnn[hH] | $nnn - for Hex\n"
"    nnn[oO] - for octal\n"
"    nnn[bB] - for binary\n"
"    nnn - for decimal (also default for non-recognized)\n"
"\n"
"    given the above you can also postpend a [MKk] for Megabyte\n"
"    Kilobyte and kilobyte respectively. [range checking inuse]";


char *opt_infile_name;
char *opt_rdb_name;
char *opt_device_name;
ulong opt_unit = -1;					  /* -1 for invalid */
ulong opt_start_block = -1;				  /* -1 for invalid. */
ulong opt_end_block = -1;				  /* -1 for invalid */
ulong opt_verbose;
ulong opt_expert;
ulong opt_debug;
ulong opt_quiet = 0;
ulong number_of_buffer_blocks = 100;

FILE *mout;
FILE *min;

int
main (int argc, char **argv)
{
    int ret = 0;
    int opt;
    int opt_quit = 0;
    int opt_version = 0;
    int opt_help = 0;
    int longind = 0;
    struct List *dl;
    FILE *file = NULL; 

    mout = stdout;
    min = stdin;

    signal (SIGINT, SIG_IGN);
    
    if (argc) {
	while (EOF != (opt = getopt_long (argc, argv, short_options,
					  long_options, &longind))) {
	    switch (opt) {
	      case 'q':
		opt_quiet = 1;
		break;
	      case 'v':
		opt_version = 1;
		opt_quit = 1;
		break;
	      case 'V':
		opt_verbose = 1;
		break;
	      case '?':
	      case 'h':
		opt_help = 1;
		opt_quit = 1;
		break;
	      case 'n':
		opt_rdb_name = optarg;
		break;
	      case 'd':
		opt_device_name = optarg;
		break;
	      case 'f':
		opt_infile_name = optarg;
		break;
	      case 'b':
		if (!(string_to_number (optarg, &number_of_buffer_blocks))) {
		    opt_quit = 1;
		    opt_help = 1;
		    ret = 20;
		}
		break;
#if defined (EXPERT_VERSION)
	      case 'x':
		opt_expert = 1;
		break;
	      case 's':
		if (!(string_to_number (optarg, &opt_start_block))) {
		    opt_quit = 1;
		    opt_help = 1;
		    ret = 20;
		}
		break;
	      case 'e':
		if (!(string_to_number (optarg, &opt_end_block))) {
		    opt_quit = 1;
		    opt_help = 1;
		    ret = 20;
		}
		break;
#endif /* EXPERT_VERSION */
	      case 'u':
		if (!(string_to_number (optarg, &opt_unit))) {
		    opt_quit = 1;
		    opt_help = 1;
		    ret = 20;
		}
		break;
	      case 'g':
		opt_debug = 1;
	    }
	}
	if (opt_quiet && opt_expert) {
	    message ("--quiet-mode (-q) and --expert-mode (-x) not allowed at same time.\n");
	    opt_quit = 1;
	    ret = 20;
	}
	if (opt_version) {
	    message (version_string, argv[0]);
	}
	if (opt_help) {
	    message (help_string, argv[0]);
	}
	if (opt_quit) {
	    return (ret);
	}
	if (!opt_infile_name) {
	    min = fopen ("*", "w+");
	    if (!min) {
		return (20);
	    }
	    file = stdin;
	}
	/* there should be NO messages before this point!! */
        dl = get_drive_list ();
	if (dl) {
	    struct partition *p = find_partition (dl, opt_device_name, opt_rdb_name,
						  opt_unit, opt_start_block,
						  opt_end_block);
	    if (p) {
		if (opt_infile_name) {
		    file = fopen (opt_infile_name, "r");
		} 
		if (file) {
		    if (!isatty (fileno(file))) {
			int def = 'N';
			ulong st, end;
			if (!opt_quiet) {
			    message ("found partition: \"%s\" capacity: %ld.%ld Megs",
				     p->name, megs (p->total_blocks*p->block_size),
				     tenths_of_a_meg (p->total_blocks*p->block_size));
			    message ("start block: %ld  end block: %ld total blocks: %ld",
				     p->start_block, p->end_block, p->total_blocks);
			    message ("block Size: %ld", p->block_size);
			}
			st = opt_start_block;
			end = opt_end_block;
			if (st == (ulong)-1) {
			    st = p->start_block;
			}
			if (end == (ulong)-1) {
			    end = p->end_block;
			}
			if (check_values (p, st, end, opt_expert)) {
			    int do_it = 0;	  /* default don't do it. */
			    if (!opt_quiet) {
				message ("dumping to: start block: %ld to end block: %ld [size: %ldK]\n",
					 st, end, ((end-st)*p->unit->bytes_per_block)/1024);
				def = ask_bool (def, 'y', "write from file \"%s\" to partition \"%s\"",
						opt_infile_name ? opt_infile_name
						: "stdin", p->name);
				if (tolower (def) == 'y') {
				    do_it = 1;
				}
			    } else {
				/* in quiet mode we always work. */
				do_it = 1;
			    }
			    if (do_it) {
				file_to_dev (p->unit->name, p->unit->unit,
					     p->unit->bytes_per_block, file,
					     st, end);
			    } else {
				message ("ok, quiting...");
			    }
			}
		    } else {
			warn_message ("Pipes and re-direction will work but interactive\n"
				      "input/output is prohibited.");
		    }
		    if (opt_infile_name) {
			fclose (file);
		    }
		}
	    } else {
		warn_message ("could not locate a partition with your specs.");
	    }		
	    free_drive_list (dl);
	}
	if (!opt_infile_name) {
	    fclose (min);
	}
    }
    return (0);
}

int
check_values (struct partition *p, ulong st, ulong end, int exp)
{
    if (st > end) {
	message ("error: Your end block [%ld] is less than your start block [%ld]!\n",
		 st,end);
	return (0);
    }
    if (st < p->start_block || st > p->end_block ||
	end > p->end_block || end < p->start_block) {
	warn_message ("ERROR: start and end blocks cannot cross partition boundries.");
	return (0);
    }
    if (st != p->start_block || end != p->end_block) {
	if (exp) {
	    message ("Please note the values you gave for start and end\n"
		     "do NOT match the partition in question.");
	} else {
	    message ("error: you must set specify the `expert-mode\' argument to gain\n"
		     "       access inside the partition (ie. not the exact\n"
		     "       same block numbers as the partition's).");
	    return (0);
	}
    }
    return (1);
}

int
check_break (void)
{
    if (SIGBREAKF_CTRL_C & SetSignal (0, SIGBREAKF_CTRL_C)) {
	return (1);
    }
    return (0);
}

void
file_to_dev (char *name, ulong unit, ulong bpb, FILE *file, ulong cb, ulong end)
{
    struct device_data *dd = alloc_device (name, unit, 0, sizeof (struct IOStdReq));
    if (dd) {
	ulong num_buffers = number_of_buffer_blocks;
	ulong total_blocks = end - cb + 1;
	ulong bw = 0, btw = 0, bytetw = 0;
	int last_write = 0;
	void *buffer = zmalloc (num_buffers*bpb);
	if (buffer) {
	    while (cb <= end && !last_write) {
		/* read from file. */
		if (!opt_quiet) {
		    fprintf (mout, "reading: %08ld -> %08ld  [%3ld%%] \r", cb,
			     cb + num_buffers - 1,
			     ((bw+(num_buffers/2))*100/total_blocks));
		    fflush (mout);
		}
		bytetw = fread (buffer, 1, bpb*num_buffers, file);
		if ((bytetw != (num_buffers*bpb)) && ferror (file)) {
		    fprintf (mout, "\n");
		    warn_message ("couldn't complete operation, read failed.");
		    break;
		} else if (bytetw == 0) {
		    break;
		}
		
		/* write to device. */
		if (bytetw != (num_buffers*bpb)) {
		    btw = bytetw/bpb + (bytetw % bpb ? 1 : 0);
		    if (bytetw % bpb) {
			warn_message ("warning non blocked input received, early termination.");
			last_write = 1;
		    }
		} else {
		    btw = num_buffers;
		}

		if (check_break ()) {
		    last_write = 1;
		}

		if ((cb + btw - 1) > end) {
		    message ("error: stream tried to overwrite device boundries, trimming.");
		    btw = end - cb + 1;
		    last_write = 1;
		}
		if (!opt_quiet) {
		    fprintf (mout, "writing: %08ld -> %08ld  [%3ld%%] \r", cb,
			     cb + num_buffers - 1,
			     ((btw+bw)*100/total_blocks));
		    fflush (mout);
		}
		if (bpb*btw != device_write (dd, cb*bpb, btw*bpb, buffer)) {
		    fprintf (mout, "\n");
		    warn_message ("couldn't complete operation, write failed.");
		    break;
		}
		bw += btw;
		cb += btw;
	    }
	    zfree (buffer);
	    fprintf (mout, "\n");
	} else {
	    warn_message ("couldn't allocate io for operation.");
	}
	free_device (dd);
    } else {
	warn_message ("couldn't open device \"%s\" unit: %ld for operation.",
		      name, unit);
    }
}

/* all the arguments, except the drive list itself, are search limiters. */
/* they are generalized with: NULL for strings and (ulong)-1 for ulongs. */
/* also the function returns as soon as all non-generalized criterion are met.*/
struct partition *
find_partition (struct List *dl, char *dev_name, char *part_name,
		ulong unit, ulong start_block, ulong end_block)
{
    struct Node *dn, *un, *pn;
    /* walk list of devices. */
    for (dn = dl->lh_Head; dn->ln_Succ; dn = dn->ln_Succ) {
	struct device *d = ptrfrom (struct device, node, dn);

	if (dev_name == NULL || (!stricmp (dev_name, d->name))) {
	    /* walk list of units. */

	    for (un = d->units.lh_Head; un->ln_Succ; un = un->ln_Succ) {
		struct unit *u = ptrfrom (struct unit, node, un);

		if (unit == (ulong)-1 || (u->unit == unit)) {
		    /* walk list of partitions. */
		    for (pn = u->parts.lh_Head; pn->ln_Succ; pn = pn->ln_Succ) {
			struct partition *p = ptrfrom (struct partition, node, pn);
			int do_it = 1;
			
			if (part_name && stricmp (p->name, part_name)) {
			    do_it = 0;
			}
			if (start_block != (ulong)-1 &&
			    (start_block < p->start_block ||
			    start_block > p->end_block)) {
			    do_it = 0;
			}
			if (end_block != (ulong)-1 &&
			    (end_block > p->end_block ||
			    end_block < p->start_block)) {
			    do_it = 0;
			}
			if (do_it) {
			    return (p);
			}
		    }
		}
	    }
	}
    }
    return (NULL);
}


