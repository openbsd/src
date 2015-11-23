/*	$OpenBSD: vmd.c,v 1.5 2015/11/23 20:18:33 reyk Exp $	*/

/*
 * Copyright (c) 2015 Mike Larkin <mlarkin@openbsd.org>
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
 * vmd(8) - virtual machine daemon
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <dev/ic/comreg.h>
#include <dev/ic/i8253reg.h>
#include <dev/isa/isareg.h>
#include <dev/pci/pcireg.h>

#include <machine/param.h>
#include <machine/vmmvar.h>

#include <errno.h>
#include <fcntl.h>
#include <imsg.h>
#include <limits.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>
#include <util.h>

#include "vmd.h"
#include "loadfile.h"
#include "pci.h"
#include "virtio.h"

#define NR_BACKLOG 5

#define MAX_TAP 256

/*
 * Emulated 8250 UART
 *
 */
#define COM1_DATA	0x3f8
#define COM1_IER	0x3f9
#define COM1_IIR	0x3fa
#define COM1_LCR	0x3fb
#define COM1_MCR	0x3fc
#define COM1_LSR	0x3fd
#define COM1_MSR	0x3fe
#define COM1_SCR	0x3ff

/*
 * Emulated i8253 PIT (counter)
 */
#define TIMER_BASE	0x40
#define TIMER_CTRL	0x43	/* 8253 Timer #1 */
#define NS_PER_TICK (1000000000 / TIMER_FREQ)

/* i8253 registers */
struct i8253_counter {
	struct timeval tv;	/* timer start time */
	uint16_t start;		/* starting value */
	uint16_t olatch;	/* output latch */
	uint16_t ilatch;	/* input latch */
	uint8_t last_r;		/* last read byte (MSB/LSB) */
	uint8_t last_w;		/* last written byte (MSB/LSB) */
};

/* ns8250 UART registers */
struct ns8250_regs {
	uint8_t lcr;		/* Line Control Register */
	uint8_t fcr;		/* FIFO Control Register */
	uint8_t iir;		/* Interrupt ID Register */
	uint8_t ier;		/* Interrupt Enable Register */
	uint8_t divlo;		/* Baud rate divisor low byte */
	uint8_t divhi;		/* Baud rate divisor high byte */
	uint8_t msr;		/* Modem Status Register */
	uint8_t lsr;		/* Line Status Register */
	uint8_t mcr;		/* Modem Control Register */
	uint8_t scr;		/* Scratch Register */
	uint8_t data;		/* Unread input data */
};

struct i8253_counter i8253_counter[3];
struct ns8250_regs com1_regs;

__dead void usage(void);

void sighdlr(int);
int main(int, char **);
int control_run(void);
int disable_vmm(void);
int enable_vmm(void);
int start_vm(struct imsg *);
int terminate_vm(struct imsg *);
int get_info_vm(struct imsgbuf *);
int start_client_vmd(void);
int opentap(void);
int run_vm(int *, int *, struct vm_create_params *);
void *vcpu_run_loop(void *);
int vcpu_exit(struct vm_run_params *);
int vmm_create_vm(struct vm_create_params *);
void init_emulated_hw(struct vm_create_params *, int *, int *);
void vcpu_exit_inout(struct vm_run_params *);
uint8_t vcpu_exit_pci(struct vm_run_params *);
void vcpu_exit_i8253(union vm_exit *);
void vcpu_exit_com(struct vm_run_params *);
void vcpu_process_com_data(union vm_exit *);
void vcpu_process_com_lcr(union vm_exit *);
void vcpu_process_com_lsr(union vm_exit *);
void vcpu_process_com_ier(union vm_exit *);
void vcpu_process_com_mcr(union vm_exit *);
void vcpu_process_com_iir(union vm_exit *);
void vcpu_process_com_msr(union vm_exit *);
void vcpu_process_com_scr(union vm_exit *);

int vmm_fd, con_fd, vm_id;
volatile sig_atomic_t quit;

SLIST_HEAD(vmstate_head, vmstate);
struct vmstate_head vmstate;

extern char *__progname;

/*
 * sighdlr
 *
 * Signal handler for TERM/INT/CHLD signals used during daemon shutdown
 *
 * Parameters:
 *  sig: signal caught
 */
void
sighdlr(int sig)
{
	switch (sig) {
	case SIGTERM:
	case SIGINT:
		/* Tell main imsg loop to exit */
		quit = 1;
		break;
	case SIGCHLD:
		while (waitpid(WAIT_ANY, 0, WNOHANG) > 0) {}
		break;
	}
}

__dead void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [-dv]\n", __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	int debug = 0, verbose = 0, c, res;

	while ((c = getopt(argc, argv, "dv")) != -1) {
		switch (c) {
		case 'd':
			debug = 2;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
		}
	}

	/* log to stderr until daemonized */
	log_init(debug ? debug : 1, LOG_DAEMON);

	/* Open /dev/vmm */
	vmm_fd = open(VMM_NODE, O_RDONLY);
	if (vmm_fd == -1)
		fatal("can't open vmm device node %s", VMM_NODE);

	setproctitle("control");

	SLIST_INIT(&vmstate);

	signal(SIGTERM, sighdlr);
	signal(SIGINT, sighdlr);
	signal(SIGCHLD, sighdlr);

	log_init(debug, LOG_DAEMON);
	log_verbose(verbose);
	log_procinit("control");

	if (!debug && daemon(1, 0) == -1)
		fatal("can't daemonize");

	res = control_run();

	if (res == -1)
		fatalx("control socket error");

	return (0);
}

/*
 * control_run
 *
 * Main control loop - establishes listening socket for incoming vmmctl(8)
 * requests and dispatches appropriate calls to vmm(4). Replies to
 * vmmctl(8) using imsg.
 *
 * Return values:
 *  0: normal exit (signal to quit received)
 *  -1: abnormal exit (various causes)
 */
int
control_run(void)
{
	struct sockaddr_un sun, c_sun;
	socklen_t len;
	int fd, connfd, n, res;
	mode_t mode, old_umask;
	char *socketpath;
	struct imsgbuf *ibuf;
	struct imsg imsg;

	/* Establish and start listening on control socket */
	socketpath = SOCKET_NAME;
	if ((fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0)) == -1) {
		log_warn("%s: socket error", __progname);
		return (-1);
	}

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	if (strlcpy(sun.sun_path, socketpath, sizeof(sun.sun_path)) >=
	    sizeof(sun.sun_path)) {
		log_warnx("%s: socket name too long", __progname);
		close(fd);
		return (-1);
	}

	if (unlink(socketpath) == -1)
		if (errno != ENOENT) {
			log_warn("%s: unlink of %s failed",
			    __progname, socketpath);
			close(fd);
			return (-1);
		}

	old_umask = umask(S_IXUSR|S_IXGRP|S_IWOTH|S_IROTH|S_IXOTH);
	mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP;

	if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		log_warn("%s: control_init: bind of %s failed",
		    __progname, socketpath);
		close(fd);
		umask(old_umask);
		return (-1);
	}

	umask(old_umask);

	if (chmod(socketpath, mode) == -1) {
		log_warn("%s: control_init: chmod of %s failed",
		    __progname, socketpath);
		close(fd);
		unlink(socketpath);
		return (-1);
	}

	if ((ibuf = malloc(sizeof(struct imsgbuf))) == NULL) {
		log_warn("%s: out of memory", __progname);
		close(fd);
		unlink(socketpath);
		return (-1);
	}

	if (listen(fd, NR_BACKLOG) == -1) {
		log_warn("%s: listen failed", __progname);
		close(fd);
		unlink(socketpath);
		return (-1);
	}

	while (!quit) {
		if ((connfd = accept4(fd, (struct sockaddr *)&c_sun, &len,
		    SOCK_CLOEXEC)) == -1) {
			log_warn("%s: accept4 error", __progname);
			close(fd);
			unlink(socketpath);
			return (-1);
		}
			
		imsg_init(ibuf, connfd);
		if ((n = imsg_read(ibuf)) == -1 || n == 0) {
			log_warnx("%s: imsg_read error, n=%d",
			    __progname, n);
			continue;
		}

		for (;;) {
			if ((n = imsg_get(ibuf, &imsg)) == -1)
				return (-1);

			if (n == 0)
				break;

			/* Process incoming message (from vmmctl(8)) */
			switch (imsg.hdr.type) {
			case IMSG_VMDOP_DISABLE_VMM_REQUEST:
				res = disable_vmm();
				imsg_compose(ibuf,
				    IMSG_VMDOP_DISABLE_VMM_RESPONSE, 0, 0, -1,
				    &res, sizeof(res));
				break;
			case IMSG_VMDOP_ENABLE_VMM_REQUEST:
				res = enable_vmm();
				imsg_compose(ibuf,
				    IMSG_VMDOP_ENABLE_VMM_RESPONSE, 0, 0, -1,
				    &res, sizeof(res));
				break;
			case IMSG_VMDOP_START_VM_REQUEST:
				res = start_vm(&imsg);
				imsg_compose(ibuf,
				    IMSG_VMDOP_START_VM_RESPONSE, 0, 0, -1,
				    &res, sizeof(res));
				break;
			case IMSG_VMDOP_TERMINATE_VM_REQUEST:
				res = terminate_vm(&imsg);
				imsg_compose(ibuf,
				    IMSG_VMDOP_TERMINATE_VM_RESPONSE, 0, 0, -1,
				    &res, sizeof(res));
				break;
			case IMSG_VMDOP_GET_INFO_VM_REQUEST:
				res = get_info_vm(ibuf);
				imsg_compose(ibuf,
				    IMSG_VMDOP_GET_INFO_VM_END_DATA, 0, 0, -1,
				    &res, sizeof(res));
				break;
			}

			while (ibuf->w.queued)
				if (msgbuf_write(&ibuf->w) <= 0 && errno !=
				    EAGAIN) {
					log_warn("%s: msgbuf_write error",
					    __progname);
					close(fd);
					close(connfd);
					unlink(socketpath);
					return (-1);
				}
			imsg_free(&imsg);
		}
		close(connfd);
	}

	signal(SIGCHLD, SIG_IGN);

	return (0);
}

/*
 * disable_vmm
 *
 * Disables VMM mode on all CPUs
 *
 * Return values:
 *  0: success
 *  !0 : ioctl to vmm(4) failed
 */
int
disable_vmm(void)
{
	if (ioctl(vmm_fd, VMM_IOC_STOP, NULL) < 0)
		return (errno);

	return (0);
}

/*
 * enable_vmm
 *
 * Enables VMM mode on all CPUs
 *
 * Return values:
 *  0: success
 *  !0 : ioctl to vmm(4) failed
 */
int
enable_vmm(void)
{
	if (ioctl(vmm_fd, VMM_IOC_START, NULL) < 0)
		return (errno);

	return (0);
}

/*
 * terminate_vm
 *
 * Requests vmm(4) to terminate the VM whose ID is provided in the
 * supplied vm_terminate_params structure (vtp->vtp_vm_id)
 *
 * Parameters
 *  imsg: The incoming imsg body whose 'data' field contains the 
 *      vm_terminate_params struct
 *
 * Return values:
 *  0: success
 *  !0 : ioctl to vmm(4) failed (eg, ENOENT if the supplied VM is not
 *      valid)
 */
int
terminate_vm(struct imsg *imsg)
{
	struct vm_terminate_params *vtp;

	vtp = (struct vm_terminate_params *)imsg->data;

	if (ioctl(vmm_fd, VMM_IOC_TERM, vtp) < 0)
                return (errno);

	return (0);
}

/*
 * opentap
 *
 * Opens the next available tap device, up to MAX_TAP.
 *
 * Returns a file descriptor to the tap node opened, or -1 if no tap
 * devices were available.
 */
int
opentap(void)
{
	int i, fd;
	char path[PATH_MAX];
	
	for (i = 0; i < MAX_TAP; i++) {
		snprintf(path, PATH_MAX, "/dev/tap%d", i);
		fd = open(path, O_RDWR | O_NONBLOCK);
		if (fd != -1)
			return (fd);
	}

	return (-1);
}

/*
 * start_vm
 *
 * Starts a new VM with the creation parameters supplied (in the incoming
 * imsg->data field). This function performs a basic sanity check on the
 * incoming parameters and then performs the following steps to complete
 * the creation of the VM:
 *
 * 1. opens the VM disk image files specified in the VM creation parameters
 * 2. opens the specified VM kernel
 * 3. creates a VM console tty pair using openpty
 * 4. forks, passing the file descriptors opened in steps 1-3 to the child
 *     vmd responsible for dropping privilege and running the VM's VCPU
 *     loops.
 *
 * Parameters:
 *  imsg: The incoming imsg body whose 'data' field is a vm_create_params
 *      struct containing the VM creation parameters.
 *
 * Return values:
 *  0: success
 *  !0 : failure - typically an errno indicating the source of the failure
 */
int
start_vm(struct imsg *imsg)
{
	struct vm_create_params *vcp;
	size_t i;
	off_t kernel_size;
	struct stat sb;
	int child_disks[VMM_MAX_DISKS_PER_VM], kernel_fd, ret, ttym_fd;
	int child_taps[VMM_MAX_NICS_PER_VM];
	int ttys_fd;
	char ptyn[32];

	vcp = (struct vm_create_params *)imsg->data;

	for (i = 0 ; i < VMM_MAX_DISKS_PER_VM; i++)
		child_disks[i] = -1;
	for (i = 0 ; i < VMM_MAX_NICS_PER_VM; i++)
		child_taps[i] = -1;

	/*
	 * XXX kernel_fd can't be global (possible race if multiple VMs
	 * being created at the same time). Probably need to move this
	 * into the child before dropping privs, or just make it local
	 * to this function?
	 */
	kernel_fd = -1;

	ttym_fd = -1;
	ttys_fd = -1;

	/* Open disk images for child */
	for (i = 0 ; i < vcp->vcp_ndisks; i++) {
		child_disks[i] = open(vcp->vcp_disks[i], O_RDWR);
		if (child_disks[i] == -1) {
			ret = errno;
			log_warn("%s: can't open %s", __progname,
			    vcp->vcp_disks[i]);
			goto err;
		}
	}

	bzero(&sb, sizeof(sb));
	if (stat(vcp->vcp_kernel, &sb) == -1) {
		ret = errno;
		log_warn("%s: can't stat kernel image %s",
		    __progname, vcp->vcp_kernel);
		goto err;
	}

	kernel_size = sb.st_size;

	/* Open kernel image */
	kernel_fd = open(vcp->vcp_kernel, O_RDONLY);
	if (kernel_fd == -1) {
		ret = errno;
		log_warn("%s: can't open kernel image %s",
		    __progname, vcp->vcp_kernel);
		goto err;	
	}

	if (openpty(&ttym_fd, &ttys_fd, ptyn, NULL, NULL) == -1) {
		ret = errno;
		log_warn("%s: openpty failed", __progname);
		goto err;
	}

	if (close(ttys_fd)) {
		ret = errno;
		log_warn("%s: close tty failed", __progname);
		goto err;
	}

	/* Open tap devices for child */
	for (i = 0 ; i < vcp->vcp_nnics; i++) {
		child_taps[i] = opentap();
		if (child_taps[i] == -1) {
			ret = errno;
			log_warn("%s: can't open tap for nic %zd",
			    __progname, i);
			goto err;
		}
	}

	/* Start child vmd for this VM (fork, chroot, drop privs) */
	ret = start_client_vmd();

	/* Start child failed? - cleanup and leave */
	if (ret == -1) {
		ret = EIO;
		goto err;
	}

	if (ret > 0) {
		/* Parent */
		for (i = 0 ; i < vcp->vcp_ndisks; i++)
			close(child_disks[i]);

		for (i = 0 ; i < vcp->vcp_nnics; i++)
			close(child_taps[i]);

		close(kernel_fd);
		close(ttym_fd);

		return (0);
	}	
	else {
		/* Child */
		setproctitle(vcp->vcp_name);
		log_procinit(vcp->vcp_name);

		log_info("%s: vm console: %s", __progname, ptyn);
		ret = vmm_create_vm(vcp);
		if (ret) {
			errno = ret;
			fatal("create vmm ioctl failed - exiting");
		}

		/* Load kernel image */
		ret = loadelf_main(kernel_fd, vcp->vcp_id, vcp->vcp_memory_size);
		if (ret) {
			errno = ret;
			fatal("failed to load kernel - exiting");
		}

		close(kernel_fd);

		con_fd = ttym_fd;
		if (fcntl(con_fd, F_SETFL, O_NONBLOCK) == -1)		
			fatal("failed to set nonblocking mode on console");

		/* Execute the vcpu run loop(s) for this VM */
		ret = run_vm(child_disks, child_taps, vcp);
		_exit(ret != 0);
	}
	
	return (ret);

err:
	for (i = 0 ; i < vcp->vcp_ndisks; i++)
		if (child_disks[i] != -1) 
			close(child_disks[i]);

	for (i = 0 ; i < vcp->vcp_nnics; i++)
		if (child_taps[i] != -1)
			close(child_taps[i]);

	if (kernel_fd != -1)
		close(kernel_fd);

	if (ttym_fd != -1)
		close(ttym_fd);

	return (ret);
}

/*
 * get_info_vm
 *
 * Returns a list of VMs known to vmm(4).
 *
 * Parameters:
 *  ibuf: the imsg ibuf in which to place the results. A new imsg will
 *      be created using this ibuf.
 *
 * Return values:
 *  0: success
 *  !0 : failure (eg, ENOMEM, EIO or another error code from vmm(4) ioctl)
 */
int
get_info_vm(struct imsgbuf *ibuf)
{
	int ret;
	size_t ct, i;
	struct ibuf *obuf;
	struct vm_info_params vip;
	struct vm_info_result *info;

	/*
	 * We issue the VMM_IOC_INFO ioctl twice, once with an input
	 * buffer size of 0, which results in vmm(4) returning the
	 * number of bytes required back to us in vip.vip_size,
	 * and then we call it again after malloc'ing the required
	 * number of bytes.
	 *
	 * It is possible that we could fail a second time (eg, if
	 * another VM was created in the instant between the two
	 * ioctls, but in that case the caller can just try again
	 * as vmm(4) will return a zero-sized list in that case.
	 */
	vip.vip_size = 0;
	info = NULL;
	ret = 0;

	/* First ioctl to see how many bytes needed (vip.vip_size) */
	if (ioctl(vmm_fd, VMM_IOC_INFO, &vip) < 0)
		return (errno);

	if (vip.vip_info_ct != 0)
		return (EIO);

	info = malloc(vip.vip_size);
	if (info == NULL)
		return (ENOMEM);

	/* Second ioctl to get the actual list */
	vip.vip_info = info;
	if (ioctl(vmm_fd, VMM_IOC_INFO, &vip) < 0) {
		ret = errno;
		free(info);
		return (ret);
	}

	/* Return info to vmmctl(4) */
	ct = vip.vip_size / sizeof(struct vm_info_result);
	for (i = 0; i < ct; i++) {
		obuf = imsg_create(ibuf, IMSG_VMDOP_GET_INFO_VM_DATA, 0, 0,
		    sizeof(struct vm_info_result));
		imsg_add(obuf, &info[i], sizeof(struct vm_info_result));
		imsg_close(ibuf, obuf);
	}
	free(info);
	return (0);
}


/*
 * start_client_vmd
 *
 * forks a copy of the parent vmd, chroots to VMD_USER's home, drops
 * privileges (changes to user VMD_USER), and returns.
 * Should the fork operation succeed, but later chroot/privsep
 * fail, the child exits.
 *
 * Return values (returns to both child and parent on success):
 *  -1 : failure
 *  0: return to child vmd returns 0
 *  !0 : return to parent vmd returns the child's pid
 */
int
start_client_vmd(void)
{
	int child_pid;
	struct passwd *pw;

	pw = getpwnam(VMD_USER);
	if (pw == NULL) {
		log_warnx("%s: no such user %s", __progname, VMD_USER);
		return (-1);
	}

	child_pid = fork();
	if (child_pid < 0)
		return (-1);

	if (!child_pid) {
		/* Child */
		if (chroot(pw->pw_dir) != 0)
			fatal("unable to chroot");
		if (chdir("/") != 0)
			fatal("unable to chdir");

		if (setgroups(1, &pw->pw_gid) == -1)
			fatal("setgroups() failed");
		if (setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) == -1)
			fatal("setresgid() failed");
		if (setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) == -1)
			fatal("setresuid() failed");

		return (0);
	}

	/* Parent */
	return (child_pid);
}

/*
 * vmm_create_vm
 *
 * Requests vmm(4) to create a new VM using the supplied creation
 * parameters. This operation results in the creation of the in-kernel
 * structures for the VM, but does not start the VM's vcpu(s).
 *
 * Parameters:
 *  vcp: vm_create_params struct containing the VM's desired creation
 *      configuration
 *
 * Return values:
 *  0: success
 *  !0 : ioctl to vmm(4) failed
 */
int
vmm_create_vm(struct vm_create_params *vcp)
{
	/* Sanity check arguments */
	if (vcp->vcp_ncpus > VMM_MAX_VCPUS_PER_VM)
		return (EINVAL);

	if (vcp->vcp_memory_size > VMM_MAX_VM_MEM_SIZE)
		return (EINVAL);

	if (vcp->vcp_ndisks > VMM_MAX_DISKS_PER_VM)
		return (EINVAL);

	if (ioctl(vmm_fd, VMM_IOC_CREATE, vcp) < 0)
		return (errno);

	return (0);
}

/*
 * init_emulated_hw
 *
 * Initializes the userspace hardware emulation
 */
void
init_emulated_hw(struct vm_create_params *vcp, int *child_disks,
    int *child_taps)
{
	/* Init the i8253 PIT's 3 counters */
	bzero(&i8253_counter, sizeof(struct i8253_counter) * 3);
	gettimeofday(&i8253_counter[0].tv, NULL);
	gettimeofday(&i8253_counter[1].tv, NULL);
	gettimeofday(&i8253_counter[2].tv, NULL);
	i8253_counter[0].start = TIMER_DIV(100);
	i8253_counter[1].start = TIMER_DIV(100);
	i8253_counter[2].start = TIMER_DIV(100);

	/* Init ns8250 UART */
	bzero(&com1_regs, sizeof(struct ns8250_regs));

	/* Initialize PCI */
	pci_init();

	/* Initialize virtio devices */
	virtio_init(vcp, child_disks, child_taps);
}

/*
 * run_vm
 *
 * Runs the VM whose creation parameters are specified in vcp
 *
 * Parameters:
 *  vcp: vm_create_params struct containing the VM's desired creation
 *      configuration
 *  child_disks: previously-opened child VM disk file file descriptors
 *  child_taps: previously-opened child tap file descriptors
 *
 * Return values:
 *  0: the VM exited normally
 *  !0 : the VM exited abnormally or failed to start
 */
int
run_vm(int *child_disks, int *child_taps, struct vm_create_params *vcp)
{
	size_t i;
	int ret;
	pthread_t *tid;
	void *exit_status;
	struct vm_run_params **vrp;

	ret = 0;

	/* XXX cap vcp_ncpus to avoid overflow here */
	/*
	 * XXX ensure nvcpus in vcp is same as vm, or fix vmm to return einval
	 * on bad vcpu id
	 */
	tid = malloc(sizeof(pthread_t) * vcp->vcp_ncpus);
	vrp = malloc(sizeof(struct vm_run_params *) * vcp->vcp_ncpus);
	if (tid == NULL || vrp == NULL) {
		log_warn("%s: memory allocation error - exiting.",
		    __progname);
		return (ENOMEM);
	}

	init_emulated_hw(vcp, child_disks, child_taps);

	/*
	 * Create and launch one thread for each VCPU. These threads may
	 * migrate between PCPUs over time; the need to reload CPU state
	 * in such situations is detected and performed by vmm(4) in the
	 * kernel.
	 */
	for (i = 0 ; i < vcp->vcp_ncpus; i++) {
		vrp[i] = malloc(sizeof(struct vm_run_params));
		if (vrp[i] == NULL) {
			log_warn("%s: memory allocation error - "
			    "exiting.", __progname);
			/* caller will exit, so skip free'ing */
			return (ENOMEM);
		}
		vrp[i]->vrp_exit = malloc(sizeof(union vm_exit));
		if (vrp[i]->vrp_exit == NULL) {
			log_warn("%s: memory allocation error - "
			    "exiting.", __progname);
			/* caller will exit, so skip free'ing */
			return (ENOMEM);
		}
		vrp[i]->vrp_vm_id = vcp->vcp_id;
		vrp[i]->vrp_vcpu_id = i;

		/* Start each VCPU run thread at vcpu_run_loop */
		ret = pthread_create(&tid[i], NULL, vcpu_run_loop, vrp[i]);
		if (ret) {
			/* caller will _exit after this return */
			return (ret);
		}
	}

	/* Wait for all the threads to exit */
	for (i = 0; i < vcp->vcp_ncpus; i++) {
		if (pthread_join(tid[i], &exit_status)) {
			log_warn("%s: failed to join thread %zd - "
			    "exiting", __progname, i);
			return (EIO);
		}

		if (exit_status != NULL) {
			log_warnx("%s: vm %d vcpu run thread %zd exited "
			    "abnormally", __progname, vcp->vcp_id, i);
			ret = EIO;
		}
	}

	return (ret);
}

/*
 * vcpu_run_loop
 *
 * Runs a single VCPU until vmm(4) requires help handling an exit,
 * or the VM terminates.
 *
 * Parameters:
 *  arg: vcpu_run_params for the VCPU being run by this thread
 *
 * Return values:
 *  NULL: the VCPU shutdown properly
 *  !NULL: error processing VCPU run, or the VCPU shutdown abnormally
 */
void *
vcpu_run_loop(void *arg)
{
	struct vm_run_params *vrp = (struct vm_run_params *)arg;
	intptr_t ret;

	vrp->vrp_continue = 0;
	vrp->vrp_injint = -1;

	for (;;) {
		if (ioctl(vmm_fd, VMM_IOC_RUN, vrp) < 0) {
			/* If run ioctl failed, exit */
			ret = errno;
			return ((void *)ret);
		}

		/* If the VM is terminating, exit normally */
		if (vrp->vrp_exit_reason == VM_EXIT_TERMINATED)
			return (NULL);

		if (vrp->vrp_exit_reason != VM_EXIT_NONE) {
			/*
			 * vmm(4) needs help handling an exit, handle in
			 * vcpu_exit.
			 */
			if (vcpu_exit(vrp))
				return ((void *)EIO);
		}
	}

	return (NULL);
}

/*
 * vcpu_exit_i8253
 *
 * Handles emulated i8253 PIT access (in/out instruction to PIT ports).
 * We don't emulate all the modes of the i8253, just the basic squarewave
 * clock.
 *
 * Parameters:
 *  vei: VM exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_exit_i8253(union vm_exit *vei)
{
	uint32_t out_data;
	uint8_t sel, rw, data;
	uint64_t ns, ticks;
	struct timeval now, delta;

	if (vei->vei.vei_port == TIMER_CTRL) {
		if (vei->vei.vei_dir == 0) { /* OUT instruction */
			out_data = vei->vei.vei_data;
			sel = out_data &
			    (TIMER_SEL0 | TIMER_SEL1 | TIMER_SEL2);
			sel = sel >> 6;
			if (sel > 2) {
				log_warnx("%s: i8253 PIT: invalid "
				    "timer selected (%d)",
				    __progname, sel);
				return;
			}
			
			rw = vei->vei.vei_data &
			    (TIMER_LATCH | TIMER_LSB |
			    TIMER_MSB | TIMER_16BIT);

			if (rw == TIMER_16BIT) {
				/*
				 * XXX this seems to be used on occasion, needs
				 * to be implemented
				 */
				log_warnx("%s: i8253 PIT: 16 bit "
				    "counter I/O not supported",
				    __progname);
				    return;
			}

			/*
			 * Since we don't truly emulate each tick of the PIT
			 * clock, when the guest asks for the timer to be
			 * latched, simulate what the counter would have been
			 * had we performed full emulation. We do this by
			 * calculating when the counter was reset vs how much
			 * time has elapsed, then bias by the counter tick
			 * rate.
			 */
			if (rw == TIMER_LATCH) {
				gettimeofday(&now, NULL);
				delta.tv_sec = now.tv_sec -
				    i8253_counter[sel].tv.tv_sec;
				delta.tv_usec = now.tv_usec -
				    i8253_counter[sel].tv.tv_usec;
				if (delta.tv_usec < 0) { 
					delta.tv_sec--;
					delta.tv_usec += 1000000;
				}
				if (delta.tv_usec > 1000000) {
					delta.tv_sec++;
					delta.tv_usec -= 1000000;
				}
				ns = delta.tv_usec * 1000 +
				    delta.tv_sec * 1000000000;
				ticks = ns / NS_PER_TICK;
				i8253_counter[sel].olatch =
				    i8253_counter[sel].start -
				    ticks % i8253_counter[sel].start;
				return;
			}

			log_warnx("%s: i8253 PIT: unsupported rw mode "
			    "%d", __progname, rw);
			return;
		} else {
			/* XXX should this return 0xff? */
			log_warnx("%s: i8253 PIT: read from control "
			    "port unsupported", __progname);
		}
	} else {
		sel = vei->vei.vei_port - (TIMER_CNTR0 + TIMER_BASE);
		if (vei->vei.vei_dir == 0) { /* OUT instruction */
			if (i8253_counter[sel].last_w == 0) {
				out_data = vei->vei.vei_data;
				i8253_counter[sel].ilatch |= (out_data << 8);
				i8253_counter[sel].last_w = 1;
			} else {
				out_data = vei->vei.vei_data;
				i8253_counter[sel].ilatch |= out_data;
				i8253_counter[sel].start =
				    i8253_counter[sel].ilatch;
				i8253_counter[sel].last_w = 0;
			}
		} else {
			if (i8253_counter[sel].last_r == 0) {
				data = i8253_counter[sel].olatch >> 8;
				vei->vei.vei_data = data;
				i8253_counter[sel].last_w = 1;
			} else {
				data = i8253_counter[sel].olatch & 0xFF;
				vei->vei.vei_data = data;
				i8253_counter[sel].last_w = 0;
			}				
		}
	}
}

/*
 * vcpu_process_com_data
 *
 * Emulate in/out instructions to the com1 (ns8250) UART data register
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_data(union vm_exit *vei)
{
	/*
	 * vei_dir == 0 : out instruction
	 *
	 * The guest wrote to the data register. Since we are emulating a
	 * no-fifo chip, write the character immediately to the pty and
	 * assert TXRDY in IIR (if the guest has requested TXRDY interrupt
	 * reporting)
	 */
	if (vei->vei.vei_dir == 0) {
		write(con_fd, &vei->vei.vei_data, 1);
		if (com1_regs.ier & 0x2) {
			/* Set TXRDY */
			com1_regs.iir |= IIR_TXRDY;
			/* Set "interrupt pending" (IIR low bit cleared) */
			com1_regs.iir &= ~0x1;
		}
	} else {
		/*
		 * vei_dir == 1 : in instruction
		 *
		 * The guest read from the data register. Check to see if
		 * there is data available (RXRDY) and if so, consume the
		 * input data and return to the guest. Also clear the
		 * interrupt info register regardless.
		 */
		if (com1_regs.lsr & LSR_RXRDY) {
			vei->vei.vei_data = com1_regs.data;
			com1_regs.data = 0x0;
			com1_regs.lsr &= ~LSR_RXRDY;
		} else {
			/* XXX should this be com1_regs.data or 0xff? */
			vei->vei.vei_data = com1_regs.data;
			log_warnx("guest reading com1 when not ready");
		}

		/* Reading the data register always clears RXRDY from IIR */
		com1_regs.iir &= ~IIR_RXRDY;

		/*
		 * Clear "interrupt pending" by setting IIR low bit to 1
		 * if no interrupt are pending
		 */
		if (com1_regs.iir == 0x0)
			com1_regs.iir = 0x1;
	}
}

/*
 * vcpu_process_com_lcr
 *
 * Emulate in/out instructions to the com1 (ns8250) UART line control register
 *
 * Paramters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_lcr(union vm_exit *vei)
{
	/*
	 * vei_dir == 0 : out instruction
	 *
	 * Write content to line control register
	 */
	if (vei->vei.vei_dir == 0) {
		com1_regs.lcr = (uint8_t)vei->vei.vei_data;
	} else {
		/*
		 * vei_dir == 1 : in instruction
		 *
		 * Read line control register
		 */
		vei->vei.vei_data = com1_regs.lcr;
	}
}

/*
 * vcpu_process_com_iir
 *
 * Emulate in/out instructions to the com1 (ns8250) UART interrupt information
 * register. Note that writes to this register actually are to a different
 * register, the FCR (FIFO control register) that we don't emulate but still
 * consume the data provided.
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_iir(union vm_exit *vei)
{
	/*
	 * vei_dir == 0 : out instruction
	 *
	 * Write to FCR
	 */
	if (vei->vei.vei_dir == 0) {
		com1_regs.fcr = vei->vei.vei_data;
	} else {
		/*
		 * vei_dir == 1 : in instruction
		 *
		 * Read IIR. Reading the IIR resets the TXRDY bit in the IIR
		 * after the data is read.
		 */
		vei->vei.vei_data = com1_regs.iir;
		com1_regs.iir &= ~IIR_TXRDY;

		/*
		 * Clear "interrupt pending" by setting IIR low bit to 1
		 * if no interrupts are pending
		 */
		if (com1_regs.iir == 0x0)
			com1_regs.iir = 0x1;
	}
}

/*
 * vcpu_process_com_mcr
 *
 * Emulate in/out instructions to the com1 (ns8250) UART modem control
 * register.
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_mcr(union vm_exit *vei)
{
	/*
	 * vei_dir == 0 : out instruction
	 *
	 * Write to MCR
	 */
	if (vei->vei.vei_dir == 0) {
		com1_regs.mcr = vei->vei.vei_data;
	} else {
		/*
		 * vei_dir == 1 : in instruction
		 *
		 * Read from MCR
		 */
		vei->vei.vei_data = com1_regs.mcr;
	}
}

/*
 * vcpu_process_com_lsr
 *
 * Emulate in/out instructions to the com1 (ns8250) UART line status register.
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_lsr(union vm_exit *vei)
{
	/*
	 * vei_dir == 0 : out instruction
	 *
	 * Write to LSR. This is an illegal operation, so we just log it and
	 * continue.
	 */
	if (vei->vei.vei_dir == 0) {
		log_warnx("%s: LSR UART write 0x%x unsupported",
		    __progname, vei->vei.vei_data);
	} else {
		/*
		 * vei_dir == 1 : in instruction
		 *
		 * Read from LSR. We always report TXRDY and TSRE since we
		 * can process output characters immediately (at any time).
		 */
		vei->vei.vei_data = com1_regs.lsr | LSR_TSRE | LSR_TXRDY;
	}
}

/*
 * vcpu_process_com_msr
 *
 * Emulate in/out instructions to the com1 (ns8250) UART modem status register.
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_msr(union vm_exit *vei)
{
	/*
	 * vei_dir == 0 : out instruction
	 *
	 * Write to MSR. This is an illegal operation, so we just log it and
	 * continue.
	 */
	if (vei->vei.vei_dir == 0) {
		log_warnx("%s: MSR UART write 0x%x unsupported",
		    __progname, vei->vei.vei_data);
	} else {
		/*
		 * vei_dir == 1 : in instruction
		 *
		 * Read from MSR. We always report DCD, DSR, and CTS.
		 */
		vei->vei.vei_data = com1_regs.lsr | MSR_DCD | MSR_DSR | MSR_CTS;
	}
}

/*
 * vcpu_process_com_scr
 *
 * Emulate in/out instructions to the com1 (ns8250) UART scratch register. The
 * scratch register is sometimes used to distinguish an 8250 from a 16450,
 * and/or used to distinguish submodels of the 8250 (eg 8250A, 8250B). We
 * simulate an "original" 8250 by forcing the scratch register to return data
 * on read that is different from what was written.
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_scr(union vm_exit *vei)
{
	/*
	 * vei_dir == 0 : out instruction
	 *
	 * Write to SCR
	 */
	if (vei->vei.vei_dir == 0) {
		com1_regs.scr = vei->vei.vei_data;
	} else {
		/*
		 * vei_dir == 1 : in instruction
		 *
		 * Read from SCR. To make sure we don't accidentally simulate
		 * a real scratch register, we negate what was written on
		 * subsequent readback.
		 */
		vei->vei.vei_data = ~com1_regs.scr;
	}
}

/*
 * vcpu_process_com_ier
 *
 * Emulate in/out instructions to the com1 (ns8250) UART interrupt enable
 * register.
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_ier(union vm_exit *vei)
{
	/*
	 * vei_dir == 0 : out instruction
	 *
	 * Write to IER
	 */
	if (vei->vei.vei_dir == 0) {
		com1_regs.ier = vei->vei.vei_data;
	} else {
		/*
		 * vei_dir == 1 : in instruction
		 *
		 * Read from IER
		 */
		vei->vei.vei_data = com1_regs.ier;
	}
}

/*
 * vcpu_exit_com
 *
 * Process com1 (ns8250) UART exits. vmd handles most basic 8250
 * features with the exception of the divisor latch (eg, no baud
 * rate support)
 *
 * Parameters:
 *  vrp: vcpu run parameters containing guest state for this exit
 */
void
vcpu_exit_com(struct vm_run_params *vrp)
{
	union vm_exit *vei = vrp->vrp_exit;

	switch(vei->vei.vei_port) {
	case COM1_LCR:
		vcpu_process_com_lcr(vei);
		break;
	case COM1_IER:
		vcpu_process_com_ier(vei);
		break;
	case COM1_IIR:
		vcpu_process_com_iir(vei);
		break;
	case COM1_MCR:
		vcpu_process_com_mcr(vei);
		break;
	case COM1_LSR:
		vcpu_process_com_lsr(vei);
		break;
	case COM1_MSR:
		vcpu_process_com_msr(vei);
		break;
	case COM1_SCR:
		vcpu_process_com_scr(vei);
		break;	
	case COM1_DATA:
		vcpu_process_com_data(vei);
		break;
	}
}

/*
 * vcpu_exit_pci
 *
 * Handle all I/O to the emulated PCI subsystem.
 *
 * Parameters:
 *  vrp: vcpu run paramters containing guest state for this exit
 *
 * Return values:
 *  0xff if no interrupt is required after this pci exit,
 *      or an interrupt vector otherwise
 */
uint8_t
vcpu_exit_pci(struct vm_run_params *vrp)
{
	union vm_exit *vei = vrp->vrp_exit;
	uint8_t intr;

	intr = 0xFF;

	switch(vei->vei.vei_port) {
	case PCI_MODE1_ADDRESS_REG:
		pci_handle_address_reg(vrp);
		break;
	case PCI_MODE1_DATA_REG:
		pci_handle_data_reg(vrp);
		break;
	case VMM_PCI_IO_BAR_BASE ... VMM_PCI_IO_BAR_END:
		intr = pci_handle_io(vrp);
		break;		
	default:
		log_warnx("%s: unknown PCI register 0x%llx",
		    __progname, (uint64_t)vei->vei.vei_port);
		break;
	}

	return (intr);
}

/*
 * vcpu_exit_inout
 *
 * Handle all I/O exits that need to be emulated in vmd. This includes the
 * i8253 PIT and the com1 ns8250 UART.
 *
 * Parameters:
 *  vrp: vcpu run parameters containing guest state for this exit
 */
void
vcpu_exit_inout(struct vm_run_params *vrp)
{
	union vm_exit *vei = vrp->vrp_exit;
	uint8_t intr;

	switch(vei->vei.vei_port) {
	case TIMER_CTRL:
	case (TIMER_CNTR0 + TIMER_BASE):
	case (TIMER_CNTR1 + TIMER_BASE):
	case (TIMER_CNTR2 + TIMER_BASE):
		vcpu_exit_i8253(vei);
		break;
	case COM1_DATA ... COM1_SCR:
		vcpu_exit_com(vrp);
		break;
	case PCI_MODE1_ADDRESS_REG:
	case PCI_MODE1_DATA_REG:
	case VMM_PCI_IO_BAR_BASE ... VMM_PCI_IO_BAR_END:
		intr = vcpu_exit_pci(vrp);
		if (intr != 0xFF)
			vrp->vrp_injint = intr;
		else
			vrp->vrp_injint = -1;
		break;
	default:
		/* IN from unsupported port gives FFs */
		if (vei->vei.vei_dir == 1)
			vei->vei.vei_data = 0xFFFFFFFF;
		break;
	}
}

/*
 * vcpu_exit
 *
 * Handle a vcpu exit. This function is called when it is determined that
 * vmm(4) requires the assistance of vmd to support a particular guest
 * exit type (eg, accessing an I/O port or device). Guest state is contained
 * in 'vrp', and will be resent to vmm(4) on exit completion.
 *
 * Upon conclusion of handling the exit, the function determines if any
 * interrupts should be injected into the guest, and sets vrp->vrp_injint
 * to the IRQ line whose interrupt should be vectored (or -1 if no interrupt
 * is to be injected).
 *
 * Parameters:
 *  vrp: vcpu run parameters containing guest state for this exit
 *
 * Return values:
 *  0: the exit was handled successfully
 *  1: an error occurred (exit not handled)
 */
int
vcpu_exit(struct vm_run_params *vrp)
{
	ssize_t sz;
	char ch;

	switch (vrp->vrp_exit_reason) {
	case VMX_EXIT_IO:
		vcpu_exit_inout(vrp);
		break;
	case VMX_EXIT_HLT:
		/*
		 * XXX handle halted state, no reason to run this vcpu again
		 * until a vm interrupt is to be injected
		 */
		break;
	default:
		log_warnx("%s: unknown exit reason %d",
		    __progname, vrp->vrp_exit_reason);
		return (1);
	}

	/* XXX interrupt priority */
	if (vionet_process_rx()) 
		vrp->vrp_injint = 9;

	/*
	 * Is there a new character available on com1?
	 * If so, consume the character, buffer it into the com1 data register
	 * assert IRQ4, and set the line status register RXRDY bit.
	 *
	 * XXX - move all this com intr checking to another function
	 */
	sz = read(con_fd, &ch, sizeof(char));
	if (sz == 1) {
		com1_regs.lsr |= LSR_RXRDY;
		com1_regs.data = ch;
		/* XXX these ier and iir bits should be IER_x and IIR_x */
		if (com1_regs.ier & 0x1) {
			com1_regs.iir |= (2 << 1);
			com1_regs.iir &= ~0x1;
		}
	}

	/*
	 * Clear "interrupt pending" by setting IIR low bit to 1 if no
	 * interrupts are pending
	 */
	/* XXX these iir magic numbers should be IIR_x */
	if ((com1_regs.iir & ~0x1) == 0x0)
		com1_regs.iir = 0x1;

	/* If pending interrupt and nothing waiting to be injected, inject */
	if ((com1_regs.iir & 0x1) == 0)
		if (vrp->vrp_injint == -1)
			vrp->vrp_injint = 0x4;
	vrp->vrp_continue = 1;

	return (0);
}

/*
 * write_page
 *
 * Pushes a page of data from 'buf' into the guest VM's memory
 * at paddr 'dst'.
 *
 * Parameters:
 *  dst: the destination paddr_t in the guest VM to push into.
 *      If there is no guest paddr mapping at 'dst', a new page will be
 *      faulted in by the VMM (provided 'dst' represents a valid paddr
 *      in the guest's address space)
 *  buf: page of data to push
 *  len: size of 'buf'
 *  do_mask: 1 to mask the destination address (for kernel load), 0 to
 *      leave 'dst' unmasked
 *
 * Return values:
 *  various return values from ioctl(VMM_IOC_WRITEPAGE), or 0 if no error
 *      occurred.
 *
 * Note - this function only handles GPAs < 4GB. 
 */
int
write_page(uint32_t dst, void *buf, uint32_t len, int do_mask)
{
	struct vm_writepage_params vwp;

	/*
	 * Mask kernel load addresses to avoid uint32_t -> uint64_t cast
	 * errors
	 */
	if (do_mask)
		dst &= 0xFFFFFFF;
	
	vwp.vwp_paddr = (paddr_t)dst;
	vwp.vwp_data = buf;
	vwp.vwp_vm_id = vm_id;
	vwp.vwp_len = len;
	if (ioctl(vmm_fd, VMM_IOC_WRITEPAGE, &vwp) < 0) {
		log_warn("writepage ioctl failed");
		return (errno);
	}
	return (0);
}

/*
 * read_page
 *
 * Reads a page of memory at guest paddr 'src' into 'buf'.
 *
 * Parameters:
 *  src: the source paddr_t in the guest VM to read from.
 *  buf: destination (local) buffer
 *  len: size of 'buf'
 *  do_mask: 1 to mask the source address (for kernel load), 0 to
 *      leave 'src' unmasked
 *
 * Return values:
 *  various return values from ioctl(VMM_IOC_READPAGE), or 0 if no error
 *      occurred.
 *
 * Note - this function only handles GPAs < 4GB.
 */
int
read_page(uint32_t src, void *buf, uint32_t len, int do_mask)
{
	struct vm_readpage_params vrp;

	/*
	 * Mask kernel load addresses to avoid uint32_t -> uint64_t cast
	 * errors
	 */
	if (do_mask)
		src &= 0xFFFFFFF;
	
	vrp.vrp_paddr = (paddr_t)src;
	vrp.vrp_data = buf;
	vrp.vrp_vm_id = vm_id;
	vrp.vrp_len = len;
	if (ioctl(vmm_fd, VMM_IOC_READPAGE, &vrp) < 0) {
		log_warn("readpage ioctl failed");
		return (errno);
	}
	return (0);
}
