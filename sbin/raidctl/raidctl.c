/*	$OpenBSD: raidctl.c,v 1.1 1999/01/11 14:49:44 niklas Exp $	*/

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

   This program is a re-write of the original rf_ctrl program 
   distributed by CMU with RAIDframe 1.1.

   This program is the user-land interface to the RAIDframe kernel
   driver in NetBSD.

 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <util.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <err.h>
#include <sys/types.h>
#include <string.h>
#include <sys/disklabel.h>
#include <machine/disklabel.h>
#include "rf_raidframe.h"

extern  char *__progname;

int     main __P((int, char *[]));
static  void do_ioctl __P((int, unsigned long, void *, char *));
static  void rf_configure __P((int, char*));
static  char *device_status __P((RF_DiskStatus_t));
static  void rf_get_device_status __P((int));
static  void rf_fail_disk __P((int, char *, int));
static  void usage __P((void));

int
main(argc,argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	int ch;
	int num_options;
	unsigned long action;
	char config_filename[PATH_MAX];
	char dev_name[PATH_MAX];
	char name[PATH_MAX];
	char component_to_fail[PATH_MAX];
	int do_recon;
	int raidID;
	int rawpart;
	int recon_percent_done;
	struct stat st;
	int fd;

	num_options = 0;
	action = 0;
	do_recon = 0;

	while ((ch = getopt(argc, argv, "c:Cf:F:rRsu")) != -1)
		switch(ch) {
		case 'c':
			strncpy(config_filename,optarg,PATH_MAX);
			action = RAIDFRAME_CONFIGURE;
			num_options++;
			break;
		case 'C':
			action = RAIDFRAME_COPYBACK;
			num_options++;
			break;
		case 'f':
			action = RAIDFRAME_FAIL_DISK;
			do_recon = 0;
			strncpy(component_to_fail, optarg, PATH_MAX);
			num_options++;
			break;
		case 'F':
			action = RAIDFRAME_FAIL_DISK;
			do_recon = 1;
			strncpy(component_to_fail, optarg, PATH_MAX);
			num_options++;
			break;
		case 'r':
			action = RAIDFRAME_REWRITEPARITY;
			num_options++;
			break;
		case 'R':
			action = RAIDFRAME_CHECKRECON;
			num_options++;
			break;
		case 's':
			action = RAIDFRAME_GET_INFO;
			num_options++;
			break;
		case 'u':
			action = RAIDFRAME_SHUTDOWN;
			num_options++;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if ((num_options > 1) || (argc == NULL)) 
		usage();

	strncpy(name,argv[0],PATH_MAX);

	if ((name[0] == '/') || (name[0] == '.')) {
		/* they've (apparently) given a full path... */
		strncpy(dev_name, name, PATH_MAX);
	} else {
		if (isdigit(name[strlen(name)-1])) {
			rawpart = getrawpartition();
			snprintf(dev_name,PATH_MAX,"/dev/%s%c",name,
				 'a'+rawpart);		
		} else {
			snprintf(dev_name,PATH_MAX,"/dev/%s",name);
		}
	}	

	if (stat(dev_name, &st) != 0) {
		fprintf(stderr,"%s: stat failure on: %s\n",
			__progname,dev_name);
		return (errno);
	}
	if (!S_ISBLK(st.st_mode) && !S_ISCHR(st.st_mode)) {
		fprintf(stderr,"%s: invalid device: %s\n",
			__progname,dev_name);
		return (EINVAL);
	}

	raidID = RF_DEV2RAIDID(st.st_rdev);

	if ((fd = open( dev_name, O_RDWR, 0640)) < 0) {
		fprintf(stderr, "%s: unable to open device file: %s\n",
			__progname, dev_name);
		exit(1);
	}
	

	switch(action) {
	case RAIDFRAME_CONFIGURE:
		rf_configure(fd, config_filename);
		break;
	case RAIDFRAME_COPYBACK:
		printf("Copyback.\n");
		do_ioctl(fd, RAIDFRAME_COPYBACK, NULL, "RAIDFRAME_COPYBACK");
		break;
	case RAIDFRAME_FAIL_DISK:
		rf_fail_disk(fd,component_to_fail,do_recon);
		break;
	case RAIDFRAME_REWRITEPARITY:
		printf("Initiating re-write of parity\n");
		do_ioctl(fd, RAIDFRAME_REWRITEPARITY, NULL, 
			 "RAIDFRAME_REWRITEPARITY");
		break;
	case RAIDFRAME_CHECKRECON:
		do_ioctl(fd, RAIDFRAME_CHECKRECON, &recon_percent_done, 
			 "RAIDFRAME_CHECKRECON");
		printf("Reconstruction is %d%% complete.\n",
		       recon_percent_done);
		break;
	case RAIDFRAME_GET_INFO:
		rf_get_device_status(fd);
		break;
	case RAIDFRAME_SHUTDOWN:
		do_ioctl(fd, RAIDFRAME_SHUTDOWN, NULL, "RAIDFRAME_SHUTDOWN");
		break;
	default:
		break;
	}

	close(fd);
	exit(0);
}

static void
do_ioctl(fd, command, arg, ioctl_name)
	int fd;
	unsigned long command;
	void *arg;
	char *ioctl_name;
{
	if (ioctl(fd, command, arg) < 0) {
		warn("ioctl (%s) failed", ioctl_name);
		exit(1);
	}
}


static void
rf_configure(fd,config_file)
	int fd;
	char *config_file;
{
	void *generic;
	RF_Config_t cfg;

	if (rf_MakeConfig( config_file, &cfg ) < 0) {
		fprintf(stderr,"%s: unable to create RAIDframe %s\n",
			__progname, "configuration structure\n");
		exit(1);
	}
	
	/* 

	   Note the extra level of redirection needed here, since
	   what we really want to pass in is a pointer to the pointer to 
	   the configuration structure. 

	 */

	generic = (void *) &cfg;
	do_ioctl(fd,RAIDFRAME_CONFIGURE,&generic,"RAIDFRAME_CONFIGURE");
#if 0
	if (ioctl(fd, RAIDFRAME_CONFIGURE, &generic) < 0) {
		warn("ioctl (RAIDFRAME_CONFIGURE): failed\n");
		exit(1);
	}
#endif
}

static char *
device_status(status)
	RF_DiskStatus_t status;
{
	static char status_string[256];

	switch (status) {
	case rf_ds_optimal:
		strcpy(status_string,"optimal");
		break;
	case rf_ds_failed:
		strcpy(status_string,"failed");
		break;
	case rf_ds_reconstructing:
		strcpy(status_string,"reconstructing");
		break;
	case rf_ds_dist_spared:
		strcpy(status_string,"dist_spared");
		break;
	case rf_ds_spared:
		strcpy(status_string,"spared");
		break;
	case rf_ds_spare:
		strcpy(status_string,"spare");
		break;
	case rf_ds_used_spare:
		strcpy(status_string,"used_spare");
		break;
	default:
		strcpy(status_string,"UNKNOWN");
		break;
	}
	return(status_string);
}

static void
rf_get_device_status(fd)
	int fd;
{
	RF_DeviceConfig_t device_config;
	void *cfg_ptr;
	int i;

	cfg_ptr = &device_config;

	do_ioctl(fd, RAIDFRAME_GET_INFO, &cfg_ptr, "RAIDFRAME_GET_INFO");

	printf("Components:\n");
	for(i=0; i < device_config.ndevs; i++) {
		printf("%20s: %s\n", device_config.devs[i].devname, 
		       device_status(device_config.devs[i].status));
	}
	if (device_config.nspares > 0) {
		printf("Spares:\n");
		for(i=0; i < device_config.nspares; i++) {
			printf("%20s [%d][%d]: %s\n",
			       device_config.spares[i].devname, 
			       device_config.spares[i].spareRow,
 			       device_config.spares[i].spareCol, 
			       device_status(device_config.spares[i].status));
		}
	} else {
		printf("No spares.\n");
	}

}

static void
rf_fail_disk(fd, component_to_fail, do_recon)
	int fd;
	char *component_to_fail;
	int do_recon;
{
	struct rf_recon_req recon_request;
	RF_DeviceConfig_t device_config;
	void *cfg_ptr;
	int i;
	int found;
	int component_num;

	component_num = -1;
		
	/* Assuming a full path spec... */
	cfg_ptr = &device_config;
	do_ioctl(fd, RAIDFRAME_GET_INFO, &cfg_ptr, 
		 "RAIDFRAME_GET_INFO");
	found = 0;
	for(i=0; i < device_config.ndevs; i++) {
		if (strncmp(component_to_fail,
			    device_config.devs[i].devname,
			    PATH_MAX)==0) {
			found = 1;
			component_num = i;
		}
	}
	if (!found) {
		fprintf(stderr,"%s: %s is not a component %s",
			__progname, component_to_fail, 
			"of this device\n");
		exit(1);
	}

	recon_request.row = component_num / device_config.cols;
	recon_request.col = component_num % device_config.cols;
	if (do_recon) {
		recon_request.flags = RF_FDFLAGS_RECON;
	} else {
		recon_request.flags = RF_FDFLAGS_NONE;
	}
	do_ioctl(fd, RAIDFRAME_FAIL_DISK, &recon_request, 
		 "RAIDFRAME_FAIL_DISK");

}

static void
usage()
{
	fprintf(stderr, "usage: %s -c config_file dev\n", __progname);
	fprintf(stderr, "       %s -C dev\n", __progname);
	fprintf(stderr, "       %s -f component dev\n", __progname);
	fprintf(stderr, "       %s -F component dev\n", __progname);
	fprintf(stderr, "       %s -r dev\n", __progname);
	fprintf(stderr, "       %s -R dev\n", __progname);
	fprintf(stderr, "       %s -s dev\n", __progname);
	fprintf(stderr, "       %s -u dev\n", __progname);
	exit(1);
	/* NOTREACHED */
}
