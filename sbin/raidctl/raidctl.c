/*	$OpenBSD: raidctl.c,v 1.24 2005/11/14 17:17:11 deraadt Exp $	*/
/*      $NetBSD: raidctl.c,v 1.27 2001/07/10 01:30:52 lukem Exp $   */

/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Greg Oster
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This program is a re-write of the original rf_ctrl program
 * distributed by CMU with RAIDframe 1.1.
 *
 * This program is the userland interface to the RAIDframe kernel
 * driver in Net/OpenBSD.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#ifdef NETBSD
#include <sys/disklabel.h>
#endif

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "rf_raidframe.h"

extern  char *__progname;

typedef struct {
	int	fd;
	int	id;
} fdidpair;

int     main(int, char *[]);
void	do_ioctl(int, u_long, void *, const char *);
static  void rf_configure(fdidpair *, char*, int);
static  const char *device_status(RF_DiskStatus_t);
static  void rf_get_device_status(fdidpair *, int);
static	void rf_output_configuration(fdidpair *, int);
static  void get_component_number(fdidpair *, char *, int *, int *);
static  void rf_fail_disk(fdidpair *, char *, int);
static  void usage(void);
static  void get_component_label(fdidpair *, char *);
static  void set_component_label(fdidpair *, char *);
static  void init_component_labels(fdidpair *, int);
static  void set_autoconfig(fdidpair *, char *);
static  void add_hot_spare(fdidpair *, char *);
static  void remove_hot_spare(fdidpair *, char *);
static  void rebuild_in_place(fdidpair *, char *);
static  void check_status(fdidpair *,int,int);
static  void check_parity(fdidpair *,int,int);
static  void do_meter(fdidpair *, int, u_long);
static  void get_bar(char *, double, int);
static  void get_time_string(char *, int);
static  int open_device(fdidpair **, char *);
static  int get_all_devices(char ***, const char *);

int verbose;
int do_all;

int
main(int argc, char *argv[])
{
	int ch;
	int num_options;
	unsigned int action;
	char config_filename[PATH_MAX];
	char name[PATH_MAX];
	char component[PATH_MAX];
	char autoconf[10];
	int do_output;
	int do_recon;
	int do_rewrite;
	int serial_number;
	int i, nfd;
	fdidpair *fds;
	int force;
	u_long meter;
	const char *actionstr;

	num_options = 0;
	action = 0;
	meter = 0;
	do_output = 0;
	do_recon = 0;
	do_rewrite = 0;
	do_all = 0;
	serial_number = 0;
	force = 0;
	actionstr = NULL;

	while ((ch = getopt(argc, argv, "a:A:Bc:C:f:F:g:GiI:l:r:R:sSpPuv"))
	       != -1)
		switch(ch) {
		case 'a':
			action = RAIDFRAME_ADD_HOT_SPARE;
			if (strlcpy(component, optarg, sizeof component) >= sizeof(component))
				errx(1, "-a arg too long");	
			num_options++;
			break;
		case 'A':
			action = RAIDFRAME_SET_AUTOCONFIG;
			if (strlcpy(autoconf, optarg, sizeof(autoconf)) >= sizeof(autoconf))
				errx(1, "-A arg too long");
			num_options++;
			break;
		case 'B':
			action = RAIDFRAME_COPYBACK;
			num_options++;
			break;
		case 'c':
			action = RAIDFRAME_CONFIGURE;
			if (strlcpy(config_filename, optarg, sizeof config_filename) >=
			      sizeof(config_filename))
				errx(1, "-c arg too long");
			force = 0;
			num_options++;
			break;
		case 'C':
			if (strlcpy(config_filename, optarg, sizeof config_filename) >=
			      sizeof(config_filename))
				errx(1, "-C arg too long");
			action = RAIDFRAME_CONFIGURE;
			force = 1;
			num_options++;
			break;
		case 'f':
			action = RAIDFRAME_FAIL_DISK;
			if (strlcpy(component, optarg, sizeof component) >= sizeof(component))
				errx(1, "-f arg too long");
			do_recon = 0;
			num_options++;
			break;
		case 'F':
			action = RAIDFRAME_FAIL_DISK;
			if (strlcpy(component, optarg, sizeof component) >= sizeof(component))
				errx(1, "-F arg too long");
			do_recon = 1;
			num_options++;
			break;
		case 'g':
			action = RAIDFRAME_GET_COMPONENT_LABEL;
			if (strlcpy(component, optarg, sizeof component) >= sizeof(component))
				errx(1, "-g arg too long");
			num_options++;
			break;
		case 'G':
			action = RAIDFRAME_GET_INFO;
			do_output = 1;
			num_options++;
			break;
		case 'i':
			action = RAIDFRAME_REWRITEPARITY;
			num_options++;
			break;
		case 'I':
			action = RAIDFRAME_INIT_LABELS;
			serial_number = atoi(optarg);
			num_options++;
			break;
		case 'l':
			action = RAIDFRAME_SET_COMPONENT_LABEL;
			if (strlcpy(component, optarg, sizeof component) >= sizeof(component))
				errx(1, "-l arg too long");
			num_options++;
			break;
		case 'r':
			action = RAIDFRAME_REMOVE_HOT_SPARE;
			if (strlcpy(component, optarg, sizeof component) >= sizeof(component))
				errx(1, "-r arg too long");
			num_options++;
			break;
		case 'R':
			if (strlcpy(component, optarg, sizeof component) >= sizeof(component))
				errx(1, "-R arg too long");
			action = RAIDFRAME_REBUILD_IN_PLACE;
			num_options++;
			break;
		case 's':
			action = RAIDFRAME_GET_INFO;
			num_options++;
			break;
		case 'S':
			action = RAIDFRAME_CHECK_RECON_STATUS_EXT;
			num_options++;
			break;
		case 'p':
			action = RAIDFRAME_CHECK_PARITY;
			num_options++;
			break;
		case 'P':
			action = RAIDFRAME_CHECK_PARITY;
			do_rewrite = 1;
			num_options++;
			break;
		case 'u':
			action = RAIDFRAME_SHUTDOWN;
			num_options++;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if ((num_options > 1) || (argc == 0))
		usage();

	if (strlcpy(name, argv[0], sizeof name) >= sizeof(name))
		errx(1, "device name too long");

	if ((nfd = open_device(&fds, name)) < 1) {
		/* No configured raid device */
		free(fds);
		return (0);
	}

	if (do_all) {
		switch(action) {
		case RAIDFRAME_ADD_HOT_SPARE:
		case RAIDFRAME_REMOVE_HOT_SPARE:
		case RAIDFRAME_CONFIGURE:
		case RAIDFRAME_SET_AUTOCONFIG:
		case RAIDFRAME_FAIL_DISK:
		case RAIDFRAME_SET_COMPONENT_LABEL:
		case RAIDFRAME_GET_COMPONENT_LABEL:
		case RAIDFRAME_INIT_LABELS:
		case RAIDFRAME_REBUILD_IN_PLACE:
			errx(1,
			    "This action doesn't work with the 'all' device");
			break;
		default:
			break;
		}
	}

	switch(action) {
	case RAIDFRAME_ADD_HOT_SPARE:
		add_hot_spare(fds, component);
		break;
	case RAIDFRAME_REMOVE_HOT_SPARE:
		remove_hot_spare(fds, component);
		break;
	case RAIDFRAME_CONFIGURE:
		rf_configure(fds, config_filename, force);
		break;
	case RAIDFRAME_SET_AUTOCONFIG:
		set_autoconfig(fds, autoconf);
		break;
	case RAIDFRAME_COPYBACK:
		i = nfd;
		while (i--) {
			do_ioctl(fds[i].fd, RAIDFRAME_COPYBACK, NULL,
				 "RAIDFRAME_COPYBACK");
		}
		actionstr = "Copyback";
		meter = RAIDFRAME_CHECK_COPYBACK_STATUS_EXT;
		break;
	case RAIDFRAME_FAIL_DISK:
		rf_fail_disk(fds, component, do_recon);
		break;
	case RAIDFRAME_SET_COMPONENT_LABEL:
		set_component_label(fds, component);
		break;
	case RAIDFRAME_GET_COMPONENT_LABEL:
		get_component_label(fds, component);
		break;
	case RAIDFRAME_INIT_LABELS:
		init_component_labels(fds, serial_number);
		break;
	case RAIDFRAME_REWRITEPARITY:
		i = nfd;
		while (i--) {
			do_ioctl(fds[i].fd, RAIDFRAME_REWRITEPARITY, NULL,
				 "RAIDFRAME_REWRITEPARITY");
		}
		actionstr = "Parity Re-Write";
		meter = RAIDFRAME_CHECK_PARITYREWRITE_STATUS_EXT;
		break;
	case RAIDFRAME_CHECK_RECON_STATUS_EXT:
		check_status(fds, nfd, 1);
		break;
	case RAIDFRAME_GET_INFO:
		if (do_output)
			rf_output_configuration(fds, nfd);
		else
			rf_get_device_status(fds, nfd);
		break;
	case RAIDFRAME_REBUILD_IN_PLACE:
		rebuild_in_place(fds, component);
		break;
	case RAIDFRAME_CHECK_PARITY:
		check_parity(fds, nfd, do_rewrite);
		break;
	case RAIDFRAME_SHUTDOWN:
		i = nfd;
		while (i--) {
			do_ioctl(fds[i].fd, RAIDFRAME_SHUTDOWN, NULL,
				 "RAIDFRAME_SHUTDOWN");
		}
		break;
	default:
		break;
	}

	if (verbose && meter) {
		sleep(3);	/* XXX give the action a chance to start */
		printf("%s status:\n", actionstr);
		do_meter(fds, nfd, meter);
	}

	i = nfd;
	while (i--)
		close(fds[i].fd);

	free(fds);
	return (0);
}

void
do_ioctl(int fd, unsigned long command, void *arg, const char *ioctl_name)
{
	if (ioctl(fd, command, arg) < 0)
		errx(1, "ioctl (%s) failed", ioctl_name);
}


static void
rf_configure(fdidpair *fds, char *config_file, int force)
{
	void *generic;
	RF_Config_t cfg;

	if (rf_MakeConfig(config_file, &cfg) != 0)
		errx(1, "unable to create RAIDframe configuration structure");
	
	cfg.force = force;

	/*
	 * Note the extra level of redirection needed here, since
	 * what we really want to pass in is a pointer to the pointer to
	 * the configuration structure.
	 */

	generic = (void *) &cfg;
	do_ioctl(fds->fd, RAIDFRAME_CONFIGURE, &generic, "RAIDFRAME_CONFIGURE");
}

static const char *
device_status(RF_DiskStatus_t status)
{

	switch (status) {
	case rf_ds_optimal:
		return ("optimal");
	case rf_ds_failed:
		return ("failed");
	case rf_ds_reconstructing:
		return ("reconstructing");
	case rf_ds_dist_spared:
		return ("dist_spared");
	case rf_ds_spared:
		return ("spared");
	case rf_ds_spare:
		return ("spare");
	case rf_ds_used_spare:
		return ("used_spare");
	default:
		return ("UNKNOWN");
	}
	/* NOTREACHED */
}

static void
rf_get_device_status(fdidpair *fds, int nfd)
{
	RF_DeviceConfig_t device_config;
	void *cfg_ptr;
	int is_clean;
	int i,j;

	cfg_ptr = &device_config;

	i = nfd;
	while (i--) {
		do_ioctl(fds[i].fd, RAIDFRAME_GET_INFO, &cfg_ptr,
			 "RAIDFRAME_GET_INFO");

		printf("raid%d Components:\n", fds[i].id);
		for (j = 0; j < device_config.ndevs; j++) {
			printf("%20s: %s\n", device_config.devs[j].devname,
			       device_status(device_config.devs[j].status));
		}
		if (device_config.nspares > 0) {
			printf("Spares:\n");
			for (j = 0; j < device_config.nspares; j++) {
				printf("%20s: %s\n",
				       device_config.spares[j].devname,
				       device_status(device_config.spares[j].status));
			}
		} else {
			printf("No spares.\n");
		}
		if (verbose) {
			for(j=0; j < device_config.ndevs; j++) {
				if (device_config.devs[j].status ==
				    rf_ds_optimal) {
					get_component_label(&fds[i],
					   device_config.devs[j].devname);
				} else {
					printf("%s status is: %s.  "
					   "Skipping label.\n",
					   device_config.devs[j].devname,
					   device_status(device_config.devs[j].status));
				}
			}

			if (device_config.nspares > 0) {
				for(j=0; j < device_config.nspares; j++) {
					if ((device_config.spares[j].status ==
					     rf_ds_optimal) ||
					    (device_config.spares[j].status ==
					     rf_ds_used_spare)) {
						get_component_label(&fds[i],
						   device_config.spares[j].devname);
					} else {
						printf("%s status is: %s.  "
						   "Skipping label.\n",
						   device_config.spares[j].devname,
						   device_status(device_config.spares[j].status));
					}		
				}
			}
		}

		do_ioctl(fds[i].fd, RAIDFRAME_CHECK_PARITY, &is_clean,
			 "RAIDFRAME_CHECK_PARITY");
		if (is_clean) {
			printf("Parity status: clean\n");
		} else {
			printf("Parity status: DIRTY\n");
		}
		check_status(&fds[i], 1, 0);
	}
}

static void
rf_output_configuration(fdidpair *fds, int nfd)
{
	RF_DeviceConfig_t device_config;
	void *cfg_ptr;
	int i,j;
	RF_ComponentLabel_t component_label;
	void *label_ptr;
	int component_num;
	int num_cols;
	char name[PATH_MAX];

	cfg_ptr = &device_config;

	i = nfd;
	while (i--) {
		snprintf(name, PATH_MAX, "/dev/raid%dc", fds[i].id);

		printf("# raidctl config file for %s\n", name);
		printf("\n");
		do_ioctl(fds[i].fd, RAIDFRAME_GET_INFO, &cfg_ptr,
			 "RAIDFRAME_GET_INFO");

		printf("START array\n");
		printf("# numRow numCol numSpare\n");
		printf("%d %d %d\n", device_config.rows, device_config.cols,
		    device_config.nspares);
		printf("\n");

		printf("START disks\n");
		for(j = 0; j < device_config.ndevs; j++)
			printf("%s\n", device_config.devs[j].devname);
		printf("\n");

		if (device_config.nspares > 0) {
			printf("START spare\n");
			for(j = 0; j < device_config.nspares; j++)
				printf("%s\n", device_config.spares[j].devname);
			printf("\n");
		}

		for(j = 0; j < device_config.ndevs; j++) {
			if (device_config.devs[j].status == rf_ds_optimal)
				break;
		}
		if (j == device_config.ndevs) {
			printf("# WARNING: no optimal components; using %s\n",
			    device_config.devs[0].devname);
			j = 0;
		}
		get_component_number(&fds[i], device_config.devs[j].devname,
		    &component_num, &num_cols);
		memset(&component_label, 0, sizeof(RF_ComponentLabel_t));
		component_label.row = component_num / num_cols;
		component_label.column = component_num % num_cols;
		label_ptr = &component_label;
		do_ioctl(fds[i].fd, RAIDFRAME_GET_COMPONENT_LABEL, &label_ptr,
			 "RAIDFRAME_GET_COMPONENT_LABEL");

		printf("START layout\n");
		printf(
		    "# sectPerSU SUsPerParityUnit SUsPerReconUnit "
		    "RAID_level_%c\n",
		    (char) component_label.parityConfig);
		printf("%d %d %d %c\n",
		    component_label.sectPerSU, component_label.SUsPerPU,
		    component_label.SUsPerRU,
		    (char) component_label.parityConfig);
		printf("\n");

		printf("START queue\n");
		printf("fifo %d\n", device_config.maxqdepth);
	}
}

static void
get_component_number(fdidpair *fds, char *component_name, int *component_number,
		     int *num_columns)
{
	RF_DeviceConfig_t device_config;
	void *cfg_ptr;
	int i;
	int found;

	*component_number = -1;
		
	/* Assuming a full path spec... */
	cfg_ptr = &device_config;
	do_ioctl(fds->fd, RAIDFRAME_GET_INFO, &cfg_ptr, "RAIDFRAME_GET_INFO");

	*num_columns = device_config.cols;

	found = 0;
	for (i = 0; i < device_config.ndevs; i++) {
		if (strncmp(component_name, device_config.devs[i].devname,
			    PATH_MAX) == 0) {
			found = 1;
			*component_number = i;
		}
	}
	if (!found) { /* maybe it's a spare? */
		for (i = 0; i < device_config.nspares; i++) {
			if (strncmp(component_name,
				    device_config.spares[i].devname,
				    PATH_MAX) == 0) {
				found = 1;
				*component_number = i + device_config.ndevs;
				/* the way spares are done should
				   really change... */
				*num_columns = device_config.cols +
					device_config.nspares;
			}
		}
	}

	if (!found)
		errx(1, "%s is not a component of this device", component_name);
}

static void
rf_fail_disk(fdidpair *fds, char *component_to_fail, int do_recon)
{
	struct rf_recon_req recon_request;
	int component_num;
	int num_cols;

	get_component_number(fds, component_to_fail, &component_num, &num_cols);

	recon_request.row = component_num / num_cols;
	recon_request.col = component_num % num_cols;
	if (do_recon) {
		recon_request.flags = RF_FDFLAGS_RECON;
	} else {
		recon_request.flags = RF_FDFLAGS_NONE;
	}
	do_ioctl(fds->fd, RAIDFRAME_FAIL_DISK, &recon_request,
		 "RAIDFRAME_FAIL_DISK");
	if (do_recon && verbose) {
		printf("Reconstruction status:\n");
		sleep(3); /* XXX give reconstruction a chance to start */
		do_meter(fds, 1, RAIDFRAME_CHECK_RECON_STATUS_EXT);
	}
}

static void
get_component_label(fdidpair *fds, char *component)
{
	RF_ComponentLabel_t component_label;
	void *label_ptr;
	int component_num;
	int num_cols;

	get_component_number(fds, component, &component_num, &num_cols);

	memset(&component_label, 0, sizeof(RF_ComponentLabel_t));
	component_label.row = component_num / num_cols;
	component_label.column = component_num % num_cols;

	label_ptr = &component_label;
	do_ioctl(fds->fd, RAIDFRAME_GET_COMPONENT_LABEL, &label_ptr,
		 "RAIDFRAME_GET_COMPONENT_LABEL");

	printf("Component label for %s:\n", component);

	printf("   Row: %d, Column: %d, Num Rows: %d, Num Columns: %d\n",
	       component_label.row, component_label.column,
	       component_label.num_rows, component_label.num_columns);
	printf("   Version: %d, Serial Number: %d, Mod Counter: %d\n",
	       component_label.version, component_label.serial_number,
	       component_label.mod_counter);
	printf("   Clean: %s, Status: %d\n",
	       component_label.clean ? "Yes" : "No",
	       component_label.status);
	printf("   sectPerSU: %d, SUsPerPU: %d, SUsPerRU: %d\n",
	       component_label.sectPerSU, component_label.SUsPerPU,
	       component_label.SUsPerRU);
	printf("   Queue size: %d, blocksize: %d, numBlocks: %d\n",
	       component_label.maxOutstanding, component_label.blockSize,
	       component_label.numBlocks);
	printf("   RAID Level: %c\n", (char) component_label.parityConfig);
	printf("   Autoconfig: %s\n",
	       component_label.autoconfigure ? "Yes" : "No");
	printf("   Root partition: %s\n",
	       component_label.root_partition ? "Yes" : "No");
	printf("   Last configured as: raid%d\n", component_label.last_unit);
}

static void
set_component_label(fdidpair *fds, char *component)
{
	RF_ComponentLabel_t component_label;
	int component_num;
	int num_cols;

	get_component_number(fds, component, &component_num, &num_cols);

	/* XXX This is currently here for testing, and future expandability */

	component_label.version = 1;
	component_label.serial_number = 123456;
	component_label.mod_counter = 0;
	component_label.row = component_num / num_cols;
	component_label.column = component_num % num_cols;
	component_label.num_rows = 0;
	component_label.num_columns = 5;
	component_label.clean = 0;
	component_label.status = 1;
	
	do_ioctl(fds->fd, RAIDFRAME_SET_COMPONENT_LABEL, &component_label,
		 "RAIDFRAME_SET_COMPONENT_LABEL");
}


static void
init_component_labels(fdidpair *fds, int serial_number)
{
	RF_ComponentLabel_t component_label;

	component_label.version = 0;
	component_label.serial_number = serial_number;
	component_label.mod_counter = 0;
	component_label.row = 0;
	component_label.column = 0;
	component_label.num_rows = 0;
	component_label.num_columns = 0;
	component_label.clean = 0;
	component_label.status = 0;
	
	do_ioctl(fds->fd, RAIDFRAME_INIT_LABELS, &component_label,
		 "RAIDFRAME_SET_COMPONENT_LABEL");
}

static void
set_autoconfig(fdidpair *fds, char *autoconf)
{
	int auto_config;
	int root_config;

	auto_config = 0;
	root_config = 0;

	if (strncasecmp(autoconf, "root", 4) == 0) {
		root_config = 1;
	}

	if ((strncasecmp(autoconf, "yes", 3) == 0) ||
	    root_config == 1) {
		auto_config = 1;
	}

	do_ioctl(fds->fd, RAIDFRAME_SET_AUTOCONFIG, &auto_config,
		 "RAIDFRAME_SET_AUTOCONFIG");

	do_ioctl(fds->fd, RAIDFRAME_SET_ROOT, &root_config,
		 "RAIDFRAME_SET_ROOT");

	printf("raid%d: Autoconfigure: %s\n", fds->id,
	       auto_config ? "Yes" : "No");

	if (root_config == 1) {
		printf("raid%d: Root: %s\n", fds->id,
		       auto_config ? "Yes" : "No");
	}
}

static void
add_hot_spare(fdidpair *fds, char *component)
{
	RF_SingleComponent_t hot_spare;

	hot_spare.row = 0;
	hot_spare.column = 0;
	strlcpy(hot_spare.component_name, component,
		sizeof(hot_spare.component_name));
	
	do_ioctl(fds->fd, RAIDFRAME_ADD_HOT_SPARE, &hot_spare,
		 "RAIDFRAME_ADD_HOT_SPARE");
}

static void
remove_hot_spare(fdidpair *fds, char *component)
{
	RF_SingleComponent_t hot_spare;
	int component_num;
	int num_cols;

	get_component_number(fds, component, &component_num, &num_cols);

	hot_spare.row = component_num / num_cols;
	hot_spare.column = component_num % num_cols;

	strlcpy(hot_spare.component_name, component,
		sizeof(hot_spare.component_name));
	
	do_ioctl(fds->fd, RAIDFRAME_REMOVE_HOT_SPARE, &hot_spare,
		 "RAIDFRAME_REMOVE_HOT_SPARE");
}

static void
rebuild_in_place(fdidpair *fds, char *component)
{
	RF_SingleComponent_t comp;
	int component_num;
	int num_cols;

	get_component_number(fds, component, &component_num, &num_cols);

	comp.row = 0;
	comp.column = component_num;
	strlcpy(comp.component_name, component, sizeof(comp.component_name));
	
	do_ioctl(fds->fd, RAIDFRAME_REBUILD_IN_PLACE, &comp,
		 "RAIDFRAME_REBUILD_IN_PLACE");

	if (verbose) {
		printf("Reconstruction status:\n");
		sleep(3); /* XXX give reconstruction a chance to start */
		do_meter(fds, 1, RAIDFRAME_CHECK_RECON_STATUS_EXT);
	}

}

static void
check_parity(fdidpair *fds, int nfd, int do_rewrite)
{
	int i, is_clean, all_dirty, was_dirty;
	int percent_done;
	char dev_name[PATH_MAX];

	all_dirty = 0;
	i = nfd;
	while (i--) {
		is_clean = 0;
		percent_done = 0;
		snprintf(dev_name, PATH_MAX, "raid%d", fds[i].id);

		do_ioctl(fds[i].fd, RAIDFRAME_CHECK_PARITY, &is_clean,
		 	"RAIDFRAME_CHECK_PARITY");
		if (is_clean) {
			printf("%s: Parity status: clean\n", dev_name);
		} else {
			all_dirty |= 1 << fds[i].id;
			printf("%s: Parity status: DIRTY\n", dev_name);
			if (do_rewrite) {
				printf("%s: Initiating re-write of parity\n",
				    dev_name);
				do_ioctl(fds[i].fd, RAIDFRAME_REWRITEPARITY,
				    NULL, "RAIDFRAME_REWRITEPARITY");
			} else {
				/* parity is wrong, and is not being fixed. */
				exit(1);
			}
		}
	}

	if (do_all)
		strncpy(dev_name, "all raid", PATH_MAX);

	was_dirty = all_dirty;
	while (all_dirty) {
		sleep(3); /* wait a bit... */
		if (verbose) {
			printf("Parity Re-write status:\n");
			do_meter(fds, nfd,
			    RAIDFRAME_CHECK_PARITYREWRITE_STATUS_EXT);
			all_dirty = 0;
		} else {
			i = nfd;
			while (i--) {
				do_ioctl(fds[i].fd,
				 	RAIDFRAME_CHECK_PARITYREWRITE_STATUS,
				 	&percent_done,
				 	"RAIDFRAME_CHECK_PARITYREWRITE_STATUS"
				 	);
				if (percent_done == 100) {
					all_dirty &= ~(1 << fds[i].id);
				}
			}
		}
	}
	if (verbose && was_dirty)
		printf("%s: Parity Re-write complete\n", dev_name);
}


static void
check_status(fdidpair *fds, int nfd, int meter)
{
	int i;
	int recon_percent_done = 0;
	int parity_percent_done = 0;
	int copyback_percent_done = 0;
	int do_recon = 0;
	int do_parity = 0;
	int do_copyback = 0;
	u_long check = 0;

	i = nfd;
	while (i--) {
		if (meter) {
			printf("raid%d Status:\n", fds[i].id);
		}
		do_ioctl(fds[i].fd, RAIDFRAME_CHECK_RECON_STATUS,
			 &recon_percent_done,
			 "RAIDFRAME_CHECK_RECON_STATUS");
		printf("Reconstruction is %d%% complete.\n",
			recon_percent_done);
		if (recon_percent_done < 100) {
			do_recon |= 1 << fds[i].id;
		}
		do_ioctl(fds[i].fd, RAIDFRAME_CHECK_PARITYREWRITE_STATUS,
			 &parity_percent_done,
			 "RAIDFRAME_CHECK_PARITYREWRITE_STATUS");
		printf("Parity Re-write is %d%% complete.\n",
			parity_percent_done);
		if (parity_percent_done < 100) {
			do_parity |= 1 << fds[i].id;
		}
		do_ioctl(fds[i].fd, RAIDFRAME_CHECK_COPYBACK_STATUS,
			 &copyback_percent_done,
			 "RAIDFRAME_CHECK_COPYBACK_STATUS");
		printf("Copyback is %d%% complete.\n",
			copyback_percent_done);
		if (copyback_percent_done < 100) {
			do_copyback |= 1 << fds[i].id;
		}
	}

	if (meter && verbose) {
		/* These 3 should be mutually exclusive at this point */
		if (do_recon) {
			printf("Reconstruction status:\n");
			check = RAIDFRAME_CHECK_RECON_STATUS_EXT;
		} else if (do_parity) {
			printf("Parity Re-write status:\n");
			check = RAIDFRAME_CHECK_PARITYREWRITE_STATUS_EXT;
		} else if (do_copyback) {
			printf("Copyback status:\n");
			check = RAIDFRAME_CHECK_COPYBACK_STATUS_EXT;
		}
		do_meter(fds, nfd, check);
	}
}

const char *tbits = "|/-\\";

static void
do_meter(fdidpair *fds, int nfd, u_long option)
{
	int percent_done;
	int start_value;
	RF_ProgressInfo_t *progressInfo;
	void *pInfoPtr;
	struct timeval start_time;
	struct timeval current_time;
	double elapsed;
	int elapsed_sec;
	int elapsed_usec;
	int progress_total, progress_completed;
	int simple_eta, last_eta;
	double rate;
	int amount;
	int tbit_value;
	char buffer[1024];
	char bar_buffer[1024];
	char eta_buffer[1024];
	int not_done;
	int i;

	not_done = 0;
	percent_done = 0;
	tbit_value = 0;
	start_value = 0;
	last_eta = 0;
	progress_total = progress_completed = 0;
	progressInfo = malloc(nfd * sizeof(RF_ProgressInfo_t));
	memset(&progressInfo[0], 0, nfd * sizeof(RF_ProgressInfo_t));

	if (gettimeofday(&start_time, NULL))
		err(1, "gettimeofday");

	current_time = start_time;

	i = nfd;
	while (i--) {
		pInfoPtr = &progressInfo[i];
		do_ioctl(fds[i].fd, option, &pInfoPtr, "");
		start_value += progressInfo[i].completed;
		progress_total += progressInfo[i].total;

		if (progressInfo[i].completed < progressInfo[i].total) {
			not_done |= 1 << i;
		}
	}

	while (not_done) {
		progress_completed = 0;
		percent_done = 0;
		amount = 0;

		i = nfd;
		while (i--) {
			pInfoPtr = &progressInfo[i];
			do_ioctl(fds[i].fd, option, &pInfoPtr, "");
			progress_completed += progressInfo[i].completed;

			if (progressInfo[i].completed >=
			    progressInfo[i].total) {
				not_done &= ~(1 << i);
			}
		}
		percent_done = (progress_completed * 100) / progress_total;
		amount = progress_completed - start_value;

		get_bar(bar_buffer, percent_done, 40);

		elapsed_sec =   current_time.tv_sec -
				start_time.tv_sec;
		elapsed_usec =  current_time.tv_usec -
				start_time.tv_usec;
		if (elapsed_usec < 0) {
			elapsed_usec += 1000000;
			elapsed_sec--;
		}

		elapsed = (double) elapsed_sec +
			  (double) elapsed_usec / 1000000.0;

		if (amount <= 0) { /* we don't do negatives (yet?) */
			amount = 0;
		}

		if (elapsed == 0)
			rate = 0.0;
		else
			rate = amount / elapsed;

		if (rate > 0.0) {
			simple_eta = (int)
				(((double)progress_total -
				  (double) progress_completed)
				 / rate);
		} else {
			simple_eta = -1;
		}

		if (simple_eta <= 0) {
			simple_eta = last_eta;
		} else {
			last_eta = simple_eta;
		}

		get_time_string(eta_buffer, simple_eta);

		snprintf(buffer, 1024,
			 "\r\t%3d%% |%s| ETA: %s %c",
			 percent_done, bar_buffer,
			 eta_buffer, tbits[tbit_value]);

		write(fileno(stdout), buffer, strlen(buffer));
		fflush(stdout);

		if (++tbit_value > 3)
			tbit_value = 0;

		if (not_done)
			sleep(2);

		if (gettimeofday(&current_time, NULL))
			err(1, "gettimeofday");
	}
	printf("\n");
}

/* 40 '*''s per line, then 40 ' ''s line. */
/* If you've got a screen wider than 160 characters, "tough" */

#define STAR_MIDPOINT 4*40
const char stars[] = "****************************************"
                     "****************************************"
                     "****************************************"
                     "****************************************"
                     "                                        "
                     "                                        "
                     "                                        "
                     "                                        ";

static void
get_bar(char *string, double percent, int max_strlen)
{
	int offset;

	if (max_strlen > STAR_MIDPOINT) {
		max_strlen = STAR_MIDPOINT;
	}
	offset = STAR_MIDPOINT -
		(int)((percent * max_strlen) / 100);
	if (offset < 0)
		offset = 0;
	snprintf(string, max_strlen, "%s", &stars[offset]);
}

static void
get_time_string(char *string, int simple_time)
{
	int minutes, seconds, hours;
	char hours_buffer[5];
	char minutes_buffer[5];
	char seconds_buffer[5];

	if (simple_time >= 0) {

		minutes = (int) simple_time / 60;
		seconds = ((int)simple_time - 60 * minutes);
		hours = minutes / 60;
		minutes = minutes - 60 * hours;
		
		if (hours > 0) {
			snprintf(hours_buffer, sizeof(hours_buffer),
				 "%02d:", hours);
		} else {
			snprintf(hours_buffer, sizeof(hours_buffer), "   ");
		}
		
		snprintf(minutes_buffer, sizeof(minutes_buffer),
			 "%02d:", minutes);
		snprintf(seconds_buffer, sizeof(seconds_buffer),
			 "%02d", seconds);
		snprintf(string, 1024, "%s%s%s",
			 hours_buffer, minutes_buffer, seconds_buffer);
	} else {
		snprintf(string, 1024, "   --:--");
	}
	
}

static int
open_device(fdidpair **devfd, char *name)
{
	int nfd, i;
 	struct stat st;
	char **devname;

	if (strcmp(name, "all") == 0) {
		do_all = 1;
		nfd = get_all_devices(&devname, "raid");
	} else {
		nfd = 1;
		if ((devname = malloc(sizeof(void*))) == NULL)
			err(1, "malloc");
		if ((devname[0] = malloc(PATH_MAX)) == NULL)
			err(1, "malloc");

		if ((name[0] == '/') || (name[0] == '.')) {
			/* they've (apparently) given a full path... */
			strlcpy(devname[0], name, PATH_MAX);
		} else {
			if (isdigit(name[strlen(name) - 1])) {
				snprintf(devname[0], PATH_MAX, "%s%s%c",
				    _PATH_DEV, name, 'a' + getrawpartition());		
			} else {
				snprintf(devname[0], PATH_MAX, "%s%s",
				    _PATH_DEV, name);
			}
		}
	}

	if ((*devfd = malloc(nfd * sizeof(fdidpair))) == NULL)
		errx(1, "malloc() error");

	i = nfd;
	while (i--) {
		if (((*devfd)[i].fd = open(devname[i], O_RDWR, 0640)) < 0)
			errx(1, "unable to open device file: %s", devname[i]);
		if (fstat((*devfd)[i].fd, &st) != 0)
			errx(errno, "fstat failure on: %s", devname[i]);
		if (!S_ISBLK(st.st_mode) && !S_ISCHR(st.st_mode))
			errx(EINVAL, "invalid device: %s", devname[i]);

		(*devfd)[i].id = RF_DEV2RAIDID(st.st_rdev);

		free(devname[i]);
	}

	if (devname != NULL)
		free(devname);

	return (nfd);
}

static int
get_all_devices(char ***diskarray, const char *genericname)
{
	int	i, numdevs, mib[2];
	size_t	len;
	char	*disks, *fp, *p;

	numdevs = 0;

	mib[0] = CTL_HW;
	mib[1] = HW_DISKNAMES;
	sysctl(mib, 2, NULL, &len, NULL, 0);
	if ((disks = malloc(len + 1)) == NULL)
		errx(1, "malloc() error");
	sysctl(mib, 2, disks, &len, NULL, 0);
	disks[len] = '\0';

	fp = disks;
	while ((fp = strstr((const char*)fp, genericname)) != NULL) {
		numdevs++;
		fp++;
	}

	*diskarray = (char**) malloc(numdevs * sizeof(void*));
	i = 0;
	fp = disks;
	while ((p = strsep(&fp, ",")) != NULL) {
		if (strstr((const char*)p, genericname) != NULL) {
			if (asprintf(&(*diskarray)[i++], "/dev/%s%c", p,
			    'a' + getrawpartition()) == -1)
				err(1, "asprintf");	
		}
	}

	free(disks);

	return (numdevs);
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: raidctl [-v] [-afFgrR component] [-BGipPsSu] [-cC config_file]\n");
	fprintf(stderr,
	    "               [-A [yes | no | root]] [-I serial_number] dev\n");
	exit(1);
	/* NOTREACHED */
}
