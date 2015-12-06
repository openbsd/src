/*	$OpenBSD: vmd.c,v 1.18 2015/12/06 01:16:22 reyk Exp $	*/

/*
 * Copyright (c) 2015 Reyk Floeter <reyk@openbsd.org>
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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/wait.h>
#include <sys/cdefs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <ctype.h>

#include "proc.h"
#include "vmd.h"

__dead void usage(void);

int	 main(int, char **);
int	 vmd_configure(void);
void	 vmd_sighdlr(int sig, short event, void *arg);
void	 vmd_shutdown(void);
int	 vmd_control_run(void);
int	 vmd_dispatch_control(int, struct privsep_proc *, struct imsg *);
int	 vmd_dispatch_vmm(int, struct privsep_proc *, struct imsg *);

struct vmd	*env;

static struct privsep_proc procs[] = {
	{ "control",	PROC_CONTROL,	vmd_dispatch_control, control },
	{ "vmm",	PROC_VMM,	vmd_dispatch_vmm, vmm },
};

int
vmd_dispatch_control(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct privsep		*ps = p->p_ps;
	int			 res = 0, cmd = 0, v = 0;
	struct vm_create_params	 vcp;
	char			*str = NULL;

	switch (imsg->hdr.type) {
	case IMSG_VMDOP_START_VM_REQUEST:
		IMSG_SIZE_CHECK(imsg, &vcp);
		memcpy(&vcp, imsg->data, sizeof(vcp));
		res = config_getvm(ps, &vcp, -1, imsg->hdr.peerid);
		if (res == -1) {
			res = errno;
			cmd = IMSG_VMDOP_START_VM_RESPONSE;
		}
		break;
	case IMSG_VMDOP_TERMINATE_VM_REQUEST:
	case IMSG_VMDOP_GET_INFO_VM_REQUEST:
		proc_forward_imsg(ps, imsg, PROC_VMM, -1);
		break;
	case IMSG_VMDOP_RELOAD:
		v = 1;
	case IMSG_VMDOP_LOAD:
		if (IMSG_DATA_SIZE(imsg) > 0)
			str = get_string((uint8_t *)imsg->data,
			    IMSG_DATA_SIZE(imsg));
		vmd_reload(v, str);
		free(str);
		break;
	default:
		return (-1);
	}

	if (cmd &&
	    proc_compose_imsg(ps, PROC_CONTROL, -1, cmd, imsg->hdr.peerid, -1,
	    &res, sizeof(res)) == -1)
		return (-1);

	return (0);
}

int
vmd_dispatch_vmm(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct vmop_start_result vmr;
	struct privsep		*ps = p->p_ps;
	int			 res = 0;
	struct vmd_vm		*vm;

	switch (imsg->hdr.type) {
	case IMSG_VMDOP_START_VM_RESPONSE:
		IMSG_SIZE_CHECK(imsg, &res);
		memcpy(&res, imsg->data, sizeof(res));
		if ((vm = vm_getbyvmid(imsg->hdr.peerid)) == NULL)
			fatalx("%s: invalid vm response", __func__);
		if (res) {
			errno = res;
			log_warn("%s: failed to start vm",
			    vm->vm_params.vcp_name);
		} else {
			log_info("%s: started vm successfully, tty %s",
			    vm->vm_params.vcp_name, vm->vm_ttyname);
		}
		/*
		 * If the peerid is -1, the request originated from
		 * the parent, not the control socket.
		 */
		if (vm->vm_peerid == (uint32_t)-1)
			break;
		vmr.vmr_result = res;
		strlcpy(vmr.vmr_ttyname, vm->vm_ttyname,
		    sizeof(vmr.vmr_ttyname));
		if (proc_compose_imsg(ps, PROC_CONTROL, -1,
		    IMSG_VMDOP_START_VM_RESPONSE,
		    vm->vm_peerid, -1, &vmr, sizeof(vmr)) == -1)
			return (-1);
		break;
	case IMSG_VMDOP_TERMINATE_VM_RESPONSE:
		IMSG_SIZE_CHECK(imsg, &res);
		proc_forward_imsg(ps, imsg, PROC_CONTROL, -1);
		break;
		break;
	case IMSG_VMDOP_GET_INFO_VM_DATA:
	case IMSG_VMDOP_GET_INFO_VM_END_DATA:
		IMSG_SIZE_CHECK(imsg, &res);
		proc_forward_imsg(ps, imsg, PROC_CONTROL, -1);
		break;
	default:
		return (-1);
	}

	return (0);
}

void
vmd_sighdlr(int sig, short event, void *arg)
{
	struct privsep	*ps = arg;
	int		 die = 0, status, fail, id;
	pid_t		 pid;
	char		*cause;
	const char	*title = "vm";

	if (privsep_process != PROC_PARENT)
		return;

	switch (sig) {
	case SIGHUP:
		log_info("%s: reload requested with SIGHUP", __func__);

		/*
		 * This is safe because libevent uses async signal handlers
		 * that run in the event loop and not in signal context.
		 */
		vmd_reload(1, NULL);
		break;
	case SIGPIPE:
		log_info("%s: ignoring SIGPIPE", __func__);
		break;
	case SIGUSR1:
		log_info("%s: ignoring SIGUSR1", __func__);
		break;
	case SIGTERM:
	case SIGINT:
		die = 1;
		/* FALLTHROUGH */
	case SIGCHLD:
		do {
			int len;

			pid = waitpid(-1, &status, WNOHANG);
			if (pid <= 0)
				continue;

			fail = 0;
			if (WIFSIGNALED(status)) {
				fail = 1;
				len = asprintf(&cause, "terminated; signal %d",
				    WTERMSIG(status));
			} else if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) != 0) {
					fail = 1;
					len = asprintf(&cause,
					    "exited abnormally");
				} else
					len = asprintf(&cause, "exited okay");
			} else
				fatalx("unexpected cause of SIGCHLD");

			if (len == -1)
				fatal("asprintf");

			for (id = 0; id < PROC_MAX; id++) {
				if (pid == ps->ps_pid[id]) {
					die = 1;
					title = ps->ps_title[id];
					break;
				}
			}
			if (fail)
				log_warnx("lost child: %s %s", title, cause);

			free(cause);
		} while (pid > 0 || (pid == -1 && errno == EINTR));

		if (die)
			vmd_shutdown();
		break;
	default:
		fatalx("unexpected signal");
	}
}

__dead void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [-dnv] [-D macro=value] [-f file]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	struct privsep	*ps;
	int		 ch;
	const char	*conffile = VMD_CONF;

	if ((env = calloc(1, sizeof(*env))) == NULL)
		fatal("calloc: env");

	while ((ch = getopt(argc, argv, "D:df:vn")) != -1) {
		switch (ch) {
		case 'D':
			if (cmdline_symset(optarg) < 0)
				log_warnx("could not parse macro definition %s",
				    optarg);
			break;
		case 'd':
			env->vmd_debug = 2;
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'v':
			env->vmd_verbose++;
			break;
		case 'n':
			env->vmd_noaction = 1;
			break;
		default:
			usage();
		}
	}

	if (env->vmd_noaction && !env->vmd_debug)
		env->vmd_debug = 1;

	/* log to stderr until daemonized */
	log_init(env->vmd_debug ? env->vmd_debug : 1, LOG_DAEMON);

	/* check for root privileges */
	if (geteuid())
		fatalx("need root privileges");

	ps = &env->vmd_ps;
	ps->ps_env = env;

	if (config_init(env) == -1)
		fatal("failed to initialize configuration");

	if ((ps->ps_pw = getpwnam(VMD_USER)) == NULL)
		fatal("unknown user %s", VMD_USER);

	/* Configure the control socket */
	ps->ps_csock.cs_name = SOCKET_NAME;
	TAILQ_INIT(&ps->ps_rcsocks);

	/* Open /dev/vmm */
	env->vmd_fd = open(VMM_NODE, O_RDWR);
	if (env->vmd_fd == -1)
		fatal("%s", VMM_NODE);

	/* Configuration will be parsed after forking the children */
	env->vmd_conffile = VMD_CONF;

	log_init(env->vmd_debug, LOG_DAEMON);
	log_verbose(env->vmd_verbose);

	if (!env->vmd_debug && daemon(0, 0) == -1)
		fatal("can't daemonize");

	setproctitle("parent");
	log_procinit("parent");

	ps->ps_ninstances = 1;

	if (!env->vmd_noaction)
		proc_init(ps, procs, nitems(procs));

	event_init();

	signal_set(&ps->ps_evsigint, SIGINT, vmd_sighdlr, ps);
	signal_set(&ps->ps_evsigterm, SIGTERM, vmd_sighdlr, ps);
	signal_set(&ps->ps_evsigchld, SIGCHLD, vmd_sighdlr, ps);
	signal_set(&ps->ps_evsighup, SIGHUP, vmd_sighdlr, ps);
	signal_set(&ps->ps_evsigpipe, SIGPIPE, vmd_sighdlr, ps);
	signal_set(&ps->ps_evsigusr1, SIGUSR1, vmd_sighdlr, ps);

	signal_add(&ps->ps_evsigint, NULL);
	signal_add(&ps->ps_evsigterm, NULL);
	signal_add(&ps->ps_evsigchld, NULL);
	signal_add(&ps->ps_evsighup, NULL);
	signal_add(&ps->ps_evsigpipe, NULL);
	signal_add(&ps->ps_evsigusr1, NULL);

	if (!env->vmd_noaction)
		proc_listen(ps, procs, nitems(procs));

	if (vmd_configure() == -1)
		fatalx("configuration failed");

	event_dispatch();

	log_debug("parent exiting");

	return (0);
}

int
vmd_configure(void)
{
	/*
	 * pledge in the parent process:
	 * stdio - for malloc and basic I/O including events.
	 * rpath - for reload to open and read the configuration files.
	 * wpath - for opening disk images and tap devices.
	 * tty - for openpty.
	 * proc - run kill to terminate its children safely.
	 * sendfd - for disks, interfaces and other fds.
	 */
	if (pledge("stdio rpath wpath proc tty sendfd", NULL) == -1)
		fatal("pledge");

	if (parse_config(env->vmd_conffile) == -1) {
		proc_kill(&env->vmd_ps);
		exit(1);
	}

	if (env->vmd_noaction) {
		fprintf(stderr, "configuration OK\n");
		proc_kill(&env->vmd_ps);
		exit(0);
	}

	return (0);
}

void
vmd_reload(int reset, const char *filename)
{
	/* Switch back to the default config file */
	if (filename == NULL || *filename == '\0')
		filename = env->vmd_conffile;

	log_debug("%s: level %d config file %s", __func__, reset, filename);

	if (reset)
		config_setreset(env, CONFIG_ALL);

	if (parse_config(filename) == -1) {
		log_debug("%s: failed to load config file %s",
		    __func__, filename);
	}
}

void
vmd_shutdown(void)
{
	proc_kill(&env->vmd_ps);
	free(env);

	log_warnx("parent terminating");
	exit(0);
}

struct vmd_vm *
vm_getbyvmid(uint32_t vmid)
{
	struct vmd_vm	*vm;

	TAILQ_FOREACH(vm, env->vmd_vms, vm_entry) {
		if (vm->vm_vmid == vmid)
			return (vm);
	}

	return (NULL);
}

void
vm_remove(struct vmd_vm *vm)
{
	unsigned int	 i;

	if (vm == NULL)
		return;

	TAILQ_REMOVE(env->vmd_vms, vm, vm_entry);

	for (i = 0; i < VMM_MAX_DISKS_PER_VM; i++) {
		if (vm->vm_disks[i] != -1)
			close(vm->vm_disks[i]);
	}
	for (i = 0; i < VMM_MAX_NICS_PER_VM; i++) {
		if (vm->vm_ifs[i] != -1)
			close(vm->vm_ifs[i]);
	}
	if (vm->vm_kernel != -1)
		close(vm->vm_kernel);
	if (vm->vm_tty != -1)
		close(vm->vm_tty);

	free(vm);
}

char *
get_string(uint8_t *ptr, size_t len)
{
	size_t	 i;

	for (i = 0; i < len; i++)
		if (!isprint(ptr[i]))
			break;

	return strndup(ptr, i);
}
