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
#include "parser.h"

extern char *__progname;
uint32_t info_id;

/*
 * start_vm
 *
 * Request vmd to start the VM defined by the supplied parameters
 *
 * Parameters:
 *  name: optional name of the VM
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
start_vm(const char *name, int memsize, int nnics, int ndisks, char **disks,
    char *kernel)
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

	if (name != NULL)
		strlcpy(vcp->vcp_name, name, VMM_MAX_NAME_LEN);
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
get_info_vm(uint32_t id)
{
	info_id = id;
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
		vir = reallocarray(vir, ct + 1,
		    sizeof(struct vm_info_result));
		if (vir == NULL) {
			*ret = ENOMEM;
			return (1);
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
create_imagefile(const char *imgfile_path, long imgsize)
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
