/*
 * Copyright (c) 2014 Mike Larkin <mlarkin@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * vmmctl(8) - control VMM subsystem
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <machine/vmmvar.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <imsg.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vmd.h"

__dead void usage(void);
int main(int, char **);
int create_imagefile(char *, long);
void enable_vmm(void);
int enable_vmm_complete(struct imsg *, int *);
void disable_vmm(void);
int disable_vmm_complete(struct imsg *, int *);
int start_vm(int, int, int, char **, char *);
int start_vm_complete(struct imsg *, int *);
void terminate_vm(uint32_t);
int terminate_vm_complete(struct imsg *, int *);
void get_info_vm(void);
int add_info(struct imsg *, int *);
void print_vm_info(struct vm_info_result *, size_t);

#define CMD_ENABLE	0x1
#define CMD_DISABLE	0x2
#define CMD_CREATE	0x4
#define CMD_START	0x8
#define CMD_TERMINATE	0x10
#define CMD_INFO	0x20

struct imsgbuf *ibuf;
extern char *__progname;
uint32_t info_id;

/*
 * usage
 *
 * print program usage. does not return
 */
__dead void
usage(void)
{
	fprintf(stderr, "usage: %s [-de]\n"
	    "       %s [-C] [-i imagefile path] [-s size in MB]\n"
	    "       %s [-S] [-m memory size][-n nr nics][-b diskfile]"
	    "[-k kernel]\n"
	    "       %s [-T[id]]\n"
	    "       %s [-I[id]]\n",
	    __progname, __progname, __progname, __progname,
	    __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	int ch, command, ret, fd, processed, n, memsize, nnics, ndisks, i;
	const char *errstr;
	char *imgfile, *sockname;
	char *disks[VMM_MAX_DISKS_PER_VM];
	char *kernel;
	long imgsize;
	struct sockaddr_un sun;
	struct imsg imsg;
	uint32_t terminate_id;

	command = 0;
	imgfile = NULL;
	imgsize = 0;
	info_id = 0;
	terminate_id = 0;
	memsize = 0;
	nnics = 0;
	ndisks = 0;
	kernel = NULL;

	for (i = 0 ; i < VMM_MAX_DISKS_PER_VM; i++) {
		disks[i] = malloc(VMM_MAX_PATH_DISK);
		if (disks[i] == NULL) {
			fprintf(stderr, "memory allocation error\n");
			exit(1);
		}

		bzero(disks[i], VMM_MAX_PATH_DISK);
	}


	while ((ch = getopt(argc, argv, "CST::I::dei:s:m:n:b:k:")) != -1) {
		switch(ch) {
		case 'C':
			/* Create imagefile command */
			if (command)
				usage();

			command = CMD_CREATE;
			break;
		case 'S':
			/* Start command */
			if (command)
				usage();

			command = CMD_START;
			break;
		case 'T':
			/* Terminate command */
			if (command)
				usage();

			command = CMD_TERMINATE;
			if (optarg != NULL) {
				terminate_id = strtonum(optarg, 1, UINT_MAX,
				    &errstr);
				if (errstr) {
					fprintf(stderr,
					    "%s: invalid VM ID (%s) specified: "
					    "%s\n", __progname, optarg, errstr);
					usage();
				}
			} else {
				fprintf(stderr, "%s: missing VM ID parameter\n",
				    __progname);
				usage();
			}

			if (terminate_id == 0) {
				fprintf(stderr, "%s: invalid vm ID supplied\n",
				    __progname);
				usage();
			}
			break;
		case 'I':
			/* Info command */
			if (command)
				usage();

			command = CMD_INFO;
			if (optarg != NULL) {
				info_id = strtonum(optarg, 1, UINT_MAX,
				    &errstr);
				if (errstr) {
					fprintf(stderr,
					    "%s: invalid VM ID (%s) specified: "
					    "%s\n", __progname, optarg, errstr);
					usage();
				}
			}
			break;
		case 'd':
			/* Disable VMM mode */
			if (command)
				usage();

			command = CMD_DISABLE;
			break;
		case 'e':
			/* Enable VMM mode */
			if (command)
				usage();

			command = CMD_ENABLE;
			break;
		case 'i':
			/* Imagefile name parameter */
			if (imgfile)
				usage();

			imgfile = strdup(optarg);
			break;
		case 's':
			/* Imagefile size parameter */
			if (imgsize != 0)
				usage();

                        imgsize = strtonum(optarg, 1, LONG_MAX, &errstr);
                        if (errstr) {
                                fprintf(stderr,
				    "%s: invalid image size (%s) specified: "
				    "%s\n", __progname, optarg, errstr);
                                usage();
                        }
                        break;
		case 'm':
			/* VM memory parameter */
			if (memsize !=0)
				usage();

			memsize = strtonum(optarg, 1, VMM_MAX_VM_MEM_SIZE,
			    &errstr);

			if (errstr) {
				fprintf(stderr," %s: invalid memory size (%s) "
				    "specified: %s\n", __progname, optarg,
				    errstr);
			}
			break;
		case 'n':
			/* VM num nics parameter */
			if (nnics !=0)
				usage();

			nnics = strtonum(optarg, 1, VMM_MAX_VM_MEM_SIZE,
			    &errstr);

			if (errstr) {
				fprintf(stderr," %s: invalid number of nics "
				    " (%s) specified: %s\n", __progname,
				    optarg, errstr);
			}
			break;
		case 'b':
			/* VM disk parameter */
			if (ndisks < VMM_MAX_DISKS_PER_VM) {
				strlcpy(disks[ndisks], optarg,
				    VMM_MAX_PATH_DISK);
				ndisks++;
			} else {
				fprintf(stderr, "%s: maximum number of disks "
				    "reached, ignoring disk %s\n", __progname,
				    optarg);
			}
			break;
		case 'k':
			/* VM kernel parameter */
			if (kernel != NULL)
				usage();

			kernel = malloc(VMM_MAX_KERNEL_PATH);
			if (kernel == NULL) {
				fprintf(stderr, "memory allocation error\n");
				exit(1);
			}

			strlcpy(kernel, optarg, VMM_MAX_KERNEL_PATH);
			break;
		default:
			usage();
		}
	}

	if (!command)
		usage();

	/* Set up comms via imsg with vmd, unless CMD_CREATE (imgfile) */
	if (command != CMD_CREATE) {
		sockname = SOCKET_NAME;
		if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
			err(1, "socket init failed");

		bzero(&sun, sizeof(sun));
		sun.sun_family = AF_UNIX;
		if (strlcpy(sun.sun_path, sockname, sizeof(sun.sun_path)) >=
		    sizeof(sun.sun_path))
			errx(1, "socket name too long");

		if (connect(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1)
			err(1, "connect failed to vmd control socket");

		if ((ibuf = malloc(sizeof(struct imsgbuf))) == NULL)
			err(1, NULL);
		imsg_init(ibuf, fd);
	}

	if (command == CMD_DISABLE)
		disable_vmm();
	
	if (command == CMD_ENABLE)
		enable_vmm();

	if (command == CMD_CREATE) {
		if (imgfile == NULL) {
			fprintf(stderr, "%s: missing -i (imagefile path) "
			    "argument\n", __progname);
			exit(1);
		}

		if (imgsize <= 0) {
			fprintf(stderr, "%s: missing/invalid -s (imgfile size) "
			    "argument\n", __progname);
			exit(1);
		}

		ret = create_imagefile(imgfile, imgsize);
		if (ret) {
			fprintf(stderr, "%s: create imagefile operation failed "
			    "(%s)\n", __progname, strerror(ret));
			exit(ret);
		} else {
			fprintf(stdout, "%s: imagefile created\n", __progname);
			exit(0);
		}
	}

	if (command == CMD_START) {
		if (memsize == 0) {
			fprintf(stderr, "%s: missing -m (memory size) "
			    "argument\n", __progname);
			exit(1);
		}

		ret = start_vm(memsize, nnics, ndisks, disks, kernel);
		if (ret) {
			fprintf(stderr, "%s: start VM operation failed "
			    "(%s)\n", __progname, strerror(ret));
			exit(ret);
		}
	}

	if (command == CMD_TERMINATE)
		terminate_vm(terminate_id);

	if (command == CMD_INFO)
		get_info_vm();

	/* Send request message */
	while (ibuf->w.queued)
		if (msgbuf_write(&ibuf->w) <= 0 && errno != EAGAIN)
			err(1, "write error");

	/*
	 * We expect vmd to send us one reply message, and that the reply
	 * message we receive will be of the proper _REPLY type.
	 *
	 * The exception to this is the -I (get info) option, where vmd
	 * may send us arbitrary number of _REPLY messages followed by
	 * and _END message to indicate no more messages are forthcoming.
	 */
	processed = 0;
	while (!processed) {
		if ((n = imsg_read(ibuf)) == -1)
			err(1, "imsg_read error");
		if (n == 0)
			errx(1, "pipe closed");

		while (!processed) {
			if ((n = imsg_get(ibuf, &imsg)) == -1)
				err(1, "imsg_get error");
			if (n == 0)
				break;

			switch (command) {
				case CMD_DISABLE:
					processed = disable_vmm_complete(&imsg,
					    &ret);
					break;
				case CMD_ENABLE:
					processed = enable_vmm_complete(&imsg,
					    &ret);
					break;
				case CMD_START:
					processed = start_vm_complete(&imsg,
					    &ret);
					break;
				case CMD_TERMINATE:
					processed = terminate_vm_complete(&imsg,
					    &ret);
					break;
				case CMD_INFO:
					processed = add_info(&imsg, &ret);
					break;
			}
			imsg_free(&imsg);
		}
	}

	return (ret);
}

/*
 * enable_vmm
 *
 * Request vmd to enable VMM mode on the machine. This will result in the
 * appropriate instruction sequence (eg, vmxon) being executed in order
 * for the CPU to later service VMs.
 */
void
enable_vmm(void)
{
	imsg_compose(ibuf, IMSG_VMDOP_ENABLE_VMM_REQUEST, 0, 0, -1, NULL, 0);
}

/*
 * enable_vmm_complete
 *
 * Callback function invoked when we are expecting an
 * IMSG_VMDOP_ENABLE_VMM_RESPONSE message indicating the completion of
 * an enable vmm operation.
 *
 * Parameters:
 *  imsg : response imsg received from vmd
 *  ret  : return value
 *
 * Return:
 *  Always 1 to indicate we have processed the return message (even if it
 *  was an incorrect/failure message).
 *
 *  The function also sets 'ret' to the error code as follows:
 *   0     : Message successfully processed
 *   EINVAL: Invalid or unexpected response from vmd
 *   EIO   : enable_vmm command failed
 */
int
enable_vmm_complete(struct imsg *imsg, int *ret)
{
	int res;

	if (imsg->hdr.type == IMSG_VMDOP_ENABLE_VMM_RESPONSE) {
		res = *(int *)imsg->data;
		if (res) {
			fprintf(stderr, "%s: enable VMM command failed (%d) - "
			    "%s\n", __progname, res, strerror(res));
			*ret = EIO;
		} else {
			fprintf(stdout, "%s: enable VMM command successful\n",
			    __progname);
			*ret = 0;
		}
	} else {
		fprintf(stderr, "%s: unexpected response received from vmd\n",
		    __progname);
		*ret = EINVAL;
	}

	return (1);
}

/*
 * disable_vmm
 *
 * Request vmd to disable VMM mode on the machine.
 */
void
disable_vmm(void)
{
	imsg_compose(ibuf, IMSG_VMDOP_DISABLE_VMM_REQUEST, 0, 0, -1, NULL, 0);
}

/*
 * disable_vmm_complete
 *
 * Callback function invoked when we are expecting an
 * IMSG_VMDOP_DISABLE_VMM_RESPONSE message indicating the completion of
 * a disable vmm operation.
 *
 * Parameters:
 *  imsg : response imsg received from vmd
 *  ret  : return value
 *
 * Return:
 *  Always 1 to indicate we have processed the return message (even if it
 *  was an incorrect/failure message) 
 *
 *  The function also sets 'ret' to the error code as follows:
 *   0     : Message successfully processed
 *   EINVAL: Invalid or unexpected response from vmd
 *   EIO   : disable_vmm command failed
 */
int
disable_vmm_complete(struct imsg *imsg, int *ret)
{
	int res;

	if (imsg->hdr.type == IMSG_VMDOP_DISABLE_VMM_RESPONSE) {
		res = *(int *)imsg->data;
		if (res) {
			fprintf(stderr, "%s: disable VMM command failed (%d) - "
			    "%s\n", __progname, res, strerror(res));
			*ret = EIO;
		} else {
			fprintf(stdout, "%s: disable VMM command successful\n",
			    __progname);
			*ret = 0;
		}
	} else {
		fprintf(stderr, "%s: unexpected response received from vmd\n",
		    __progname);
		*ret = EINVAL;
	}

	return (1);
}

/*
 * start_vm
 *
 * Request vmd to start the VM defined by the supplied parameters
 *
 * Parameters:
 *  memsize: memory size (MB) of the VM to create
 *  nnics: number of vionet network interfaces to create
 *  ndisks: number of disk images
 *  disks: disk image file names
 *  kernel: kernel image to load
 *
 * Return:
 *  0 if the request to start the VM was sent successfully.
 *  ENOMEM if a memory allocation failure occurred.
 */
int
start_vm(int memsize, int nnics, int ndisks, char **disks, char *kernel)
{
	struct vm_create_params *vcp;
	int i;

	vcp = malloc(sizeof(struct vm_create_params));
	if (vcp == NULL)
		return (ENOMEM);

	bzero(vcp, sizeof(struct vm_create_params));

	vcp->vcp_memory_size = memsize;
	vcp->vcp_ncpus = 1;
	vcp->vcp_ndisks = ndisks;

	for (i = 0 ; i < ndisks; i++)
		strlcpy(vcp->vcp_disks[i], disks[i], VMM_MAX_PATH_DISK);

	strlcpy(vcp->vcp_kernel, kernel, VMM_MAX_KERNEL_PATH);		
	vcp->vcp_nnics = nnics;

	imsg_compose(ibuf, IMSG_VMDOP_START_VM_REQUEST, 0, 0, -1,
	    vcp, sizeof(struct vm_create_params));

	free(vcp);
	return (0);
}

/*
 * start_vm_complete
 *
 * Callback function invoked when we are expecting an
 * IMSG_VMDOP_START_VMM_RESPONSE message indicating the completion of
 * a start vm operation.
 *
 * Parameters:
 *  imsg : response imsg received from vmd
 *  ret  : return value
 *
 * Return:
 *  Always 1 to indicate we have processed the return message (even if it
 *  was an incorrect/failure message) 
 *
 *  The function also sets 'ret' to the error code as follows:
 *   0     : Message successfully processed
 *   EINVAL: Invalid or unexpected response from vmd
 *   EIO   : start_vm command failed
 */
int
start_vm_complete(struct imsg *imsg, int *ret)
{
	int res;

	if (imsg->hdr.type == IMSG_VMDOP_START_VM_RESPONSE) {
		res = *(int *)imsg->data;
		if (res) {
			fprintf(stderr, "%s: start VM command failed (%d) - "
			    "%s\n", __progname, res, strerror(res));
			*ret = EIO;	
		} else {
			fprintf(stdout, "%s: start VM command successful\n",
			    __progname);
			*ret = 0;
		}
	} else {
		fprintf(stderr, "%s: unexpected response received from vmd\n",
		    __progname);
		*ret = EINVAL;
	}

	return (1);
}

/*
 * terminate_vm
 *
 * Request vmd to stop the VM indicated
 *
 * Parameters:
 *  terminate_id: ID of the vm to be terminated
 */
void
terminate_vm(uint32_t terminate_id)
{
	struct vm_terminate_params vtp;

	bzero(&vtp, sizeof(struct vm_terminate_params));
	vtp.vtp_vm_id = terminate_id;

	imsg_compose(ibuf, IMSG_VMDOP_TERMINATE_VM_REQUEST, 0, 0, -1,
	    &vtp, sizeof(struct vm_terminate_params));
}
 
/*
 * terminate_vm_complete
 *
 * Callback function invoked when we are expecting an
 * IMSG_VMDOP_TERMINATE_VMM_RESPONSE message indicating the completion of
 * a terminate vm operation.
 *
 * Parameters:
 *  imsg : response imsg received from vmd
 *  ret  : return value
 *
 * Return:
 *  Always 1 to indicate we have processed the return message (even if it
 *  was an incorrect/failure message) 
 *
 *  The function also sets 'ret' to the error code as follows:
 *   0     : Message successfully processed
 *   EINVAL: Invalid or unexpected response from vmd
 *   EIO   : terminate_vm command failed
 */
int
terminate_vm_complete(struct imsg *imsg, int *ret)
{
	int res;

	if (imsg->hdr.type == IMSG_VMDOP_TERMINATE_VM_RESPONSE) {
		res = *(int *)imsg->data;
 		if (res) {
			fprintf(stderr, "%s: terminate VM command failed "
			    "(%d) - %s\n", __progname, res, strerror(res));
			*ret = EIO;
		} else {
			fprintf(stderr, "%s: terminate VM command successful\n",
			    __progname);
			*ret = 0;
		}
	} else {
		fprintf(stderr, "%s: unexpected response received from vmd\n",
		    __progname);
		*ret = EINVAL;
	}

	return (1);
}

/*
 * get_info_vm
 *
 * Request a list of running VMs from vmd
 */
void
get_info_vm(void)
{
	imsg_compose(ibuf, IMSG_VMDOP_GET_INFO_VM_REQUEST, 0, 0, -1, NULL, 0);
}

/*
 * add_info
 *
 * Callback function invoked when we are expecting an
 * IMSG_VMDOP_GET_INFO_VM_DATA message indicating the receipt of additional
 * "list vm" data, or an IMSG_VMDOP_GET_INFO_VM_END_DATA message indicating
 * that no additional "list vm" data will be forthcoming.
 *
 * Parameters:
 *  imsg : response imsg received from vmd
 *  ret  : return value
 *
 * Return:
 *  0     : the returned data was successfully added to the "list vm" data.
 *          The caller can expect more data.
 *  1     : IMSG_VMDOP_GET_INFO_VM_END_DATA received (caller should not call
 *          add_info again), or an error occurred adding the returned data
 *          to the "list vm" data. The caller should check the value of
 *          'ret' to determine which case occurred.
 *
 *  The function also sets 'ret' to the error code as follows:
 *   0     : Message successfully processed
 *   EINVAL: Invalid or unexpected response from vmd
 *   ENOMEM: memory allocation failure
 */
int
add_info(struct imsg *imsg, int *ret)
{
	static size_t ct = 0;
	static struct vm_info_result *vir = NULL;

	if (imsg->hdr.type == IMSG_VMDOP_GET_INFO_VM_DATA) {
		if (ct == 0) {
			vir = malloc(sizeof(struct vm_info_result));
			if (vir == NULL) {
				*ret = ENOMEM;
				return (1);
			}
		} else {
			vir = reallocarray(vir, ct + 1,
			    sizeof(struct vm_info_result));
			if (vir == NULL) {
				*ret = ENOMEM;
				return (1);
			}
		}
		bcopy(imsg->data, &vir[ct], sizeof(struct vm_info_result));
		ct++;
		*ret = 0;
		return (0);
	} else if (imsg->hdr.type == IMSG_VMDOP_GET_INFO_VM_END_DATA) {
		print_vm_info(vir, ct);
		free(vir);
		*ret = 0;
		return (1);
	} else {
		*ret = EINVAL;
		return (1);
	}
}

/*
 * print_vm_info
 *
 * Prints the vm information returned from vmd in 'list' to stdout.
 *
 * Parameters
 *  list: the vm information (consolidated) returned from vmd via imsg
 *  ct  : the size (number of elements in 'list') of the result
 */
void
print_vm_info(struct vm_info_result *list, size_t ct)
{
	size_t i, j;
	char *vcpu_state;

	printf("%5s %5s %5s %7s   %s\n", "ID", "PID", "VCPUS", "MAXMEM",
	    "NAME");
	for (i = 0; i < ct; i++) {
		if (info_id == 0 || info_id == list[i].vir_id)
			printf("%5u %5u %5zd %7zdMB %s\n",
			    list[i].vir_id, list[i].vir_creator_pid,
			    list[i].vir_ncpus, list[i].vir_memory_size,
			    list[i].vir_name);
		if (info_id == list[i].vir_id) {
			for (j = 0; j < list[i].vir_ncpus; j++) {
				if (list[i].vir_vcpu_state[j] ==
				    VCPU_STATE_STOPPED)
					vcpu_state = "STOPPED";
				else if (list[i].vir_vcpu_state[j] ==
				    VCPU_STATE_RUNNING)
					vcpu_state = "RUNNING";
				else
					vcpu_state = "UNKNOWN";

				printf(" VCPU: %2zd STATE: %s\n",
				    j, vcpu_state);
			}
		}
	}
}

/*
 * create_imagefile
 *
 * Create an empty imagefile with the specified path and size.
 *
 * Parameters:
 *  imgfile_path: path to the image file to create
 *  imgsize     : size of the image file to create
 *
 * Return:
 *  EEXIST: The requested image file already exists
 *  0     : Image file successfully created
 *  Exxxx : Various other Exxxx errno codes due to other I/O errors
 */
int
create_imagefile(char *imgfile_path, long imgsize)
{
	int fd, ret;
	struct stat sb;
	off_t ofs;
	char ch = '\0';

	/* Refuse to overwrite an existing image */
	bzero(&sb, sizeof(sb));
	if (stat(imgfile_path, &sb) == 0) {
		return (EEXIST);
	}

	fd = open(imgfile_path, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

	if (fd == -1) {
		return (errno);
	}

	ofs = (imgsize * 1024 * 1024) - 1;

	/* Set fd pos at desired size */
	if (lseek(fd, ofs, SEEK_SET) == -1) {
		ret = errno;
		close(fd);
		return (ret);
	}

	/* Write one byte to fill out the extent */
	if (write(fd, &ch, 1) == -1) {
		ret = errno;
		close(fd);
		return (ret);
	}

	ret = close(fd);	
	return (ret);
}
