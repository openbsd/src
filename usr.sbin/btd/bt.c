/*	$OpenBSD: bt.c,v 1.4 2008/11/26 15:32:56 uwe Exp $ */

/*
 * Copyright (c) 2008 Uwe Stuehler <uwe@openbsd.org>
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2004 Alexander Guy <alexander.guy@andern.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <dev/bluetooth/btdev.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <usbhid.h>

#include "btd.h"

void bt_sighdlr(int, short, void *);

struct bt_interface *bt_find_inquiry_interface(struct btd *);
struct bt_interface *bt_find_attach_interface(struct bt_device *);

void bt_check_devices(int, short, void *);

int bt_get_devinfo(struct bt_device *);
int bt_device_attach(struct bt_device *);
int bt_device_detach(struct bt_device *);

struct event ev_sigint;
struct event ev_sigterm;
struct event ev_siginfo;
struct event ev_check_devices;

int priv_fd;

void
bt_sighdlr(int sig, short ev, void *arg)
{
	struct btd *env = arg;

	switch (sig) {
	case SIGINT:
	case SIGTERM:
		(void)event_loopexit(NULL);
		break;

	case SIGINFO:
		conf_dump(env);
		break;
	}
}

pid_t
bt_main(int pipe_prnt[2], struct btd *env, struct passwd *pw)
{
	struct stat stb;
	pid_t pid;

	switch (pid = fork()) {
	case -1:
		fatal("cannot fork");
		break;
	case 0:
		break;
	default:
		return pid;
	}

	setproctitle("bt engine");

	event_init();
	hid_init(NULL);
	db_open(BTD_DB, &env->db);
	hci_init(env);
	sdp_init(env);
	control_init(env);

	if (stat(pw->pw_dir, &stb) == -1)
		fatal("stat");
	if (stb.st_uid != 0 || (stb.st_mode & (S_IWGRP|S_IWOTH)) != 0)
		fatalx("bad privsep dir permissions");
	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	close(pipe_prnt[0]);

	priv_fd = pipe_prnt[1];

	signal_set(&ev_sigint, SIGINT, bt_sighdlr, env);
	signal_set(&ev_sigterm, SIGTERM, bt_sighdlr, env);
	signal_set(&ev_siginfo, SIGINFO, bt_sighdlr, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_siginfo, NULL);

	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGCHLD, SIG_DFL);

	evtimer_set(&ev_check_devices, bt_check_devices, env);

	log_info("bt engine ready");

	(void)event_dispatch();

	log_info("bt engine exiting");
	exit(0);
}

void
bt_priv_msg(enum imsg_type type)
{
	bt_priv_send(&type, sizeof(type));
}

void
bt_priv_send(const void *buf, size_t n)
{
	if (atomic_write(priv_fd, buf, n) < 0)
		fatal("atomic_write");
}

void
bt_priv_recv(void *buf, size_t n)
{
	if (atomic_read(priv_fd, buf, n) < 0)
		fatal("atomic_read");
}

struct bt_interface *
bt_find_inquiry_interface(struct btd *env)
{
	struct bt_interface *iface;

	TAILQ_FOREACH(iface, &env->interfaces, entry) {
		if (iface->physif != NULL)
			return iface;
	}

	return NULL;
}

struct bt_interface *
bt_find_attach_interface(struct bt_device *btdev)
{
	struct bt_interface *iface;

	TAILQ_FOREACH_REVERSE(iface, &btdev->env->interfaces,
	    interfaces, entry) {
		if (iface->physif != NULL)
			return iface;
	}

	return NULL;
}

void
bt_devices_changed(void)
{
	struct timeval tv;

	memset(&tv, 0, sizeof(tv));
	evtimer_add(&ev_check_devices, &tv);
}

void
bt_check_devices(int fd, short evflags, void *arg)
{
	struct btd *env = arg;
	struct bt_device *btdev;

	for (btdev = TAILQ_FIRST(&env->devices); btdev != NULL;) {
		if ((btdev->flags & (BTDF_ATTACH|BTDF_ATTACH_DONE)) ==
		    BTDF_ATTACH_DONE) {
			if (bt_device_detach(btdev) < 0) {
				btdev = TAILQ_NEXT(btdev, entry);
				continue;
			}

			if (!(btdev->flags & BTDF_ATTACH))
				log_info("%s: device is now detached",
				    bt_ntoa(&btdev->addr, NULL));
		}

		if (btdev->flags & BTDF_DELETED) {
			struct bt_device *next;

			next = TAILQ_NEXT(btdev, entry);
			conf_delete_device(btdev);
			btdev = next;
			continue;
		}

		if ((btdev->flags & (BTDF_ATTACH|BTDF_ATTACH_DONE)) ==
		    BTDF_ATTACH) {
			if (bt_device_attach(btdev) < 0) {
				btdev = TAILQ_NEXT(btdev, entry);
				continue;
			}

			if (btdev->flags & BTDF_ATTACH_DONE)
				log_info("%s: device is now attached",
				    bt_ntoa(&btdev->addr, NULL));
		}

		btdev = TAILQ_NEXT(btdev, entry);
	}
}

int
bt_get_devinfo(struct bt_device *btdev)
{
	struct btd *env = btdev->env;
	int res;

	res = db_get_devinfo(&env->db, &btdev->addr, &btdev->info);
	if (res < 0)
		return -1;
	else if (res != 0) {
		/* try sdp query */
		return 0;
	}

	bdaddr_copy(&btdev->info.baa.bd_raddr, &btdev->addr);
	btdev->flags |= BTDF_DEVINFO_VALID;
	return 0;
}

int
bt_device_attach(struct bt_device *btdev)
{
	struct bt_interface *iface;
	void *buf;
	size_t n;
	int err;

	if (btdev->flags & BTDF_ATTACH_DONE)
		return 0;

	if (!(btdev->flags & BTDF_DEVINFO_VALID)) {
		if (bt_get_devinfo(btdev) < 0)
			return -1;
		if (!(btdev->flags & BTDF_DEVINFO_VALID))
			return 0;
	}

	if (bdaddr_any(&btdev->info.baa.bd_laddr)) {
		if ((iface = bt_find_attach_interface(btdev)) == NULL)
			return 0;

		bdaddr_copy(&btdev->info.baa.bd_laddr, &iface->addr);
	}

	if (devinfo_store(&btdev->info, &buf, &n) < 0)
		return -1;

	bt_priv_msg(IMSG_ATTACH);
	bt_priv_send(&n, sizeof(n));
	bt_priv_send(buf, n);
	bt_priv_recv(&err, sizeof(int));
	free(buf);

	switch (err) {
	case 0:
	case EADDRINUSE:
		btdev->flags |= BTDF_ATTACH_DONE;
		return 0;
	default:
		log_warnx("could not attach %s (%s)",
		    bt_ntoa(&btdev->addr, NULL), strerror(err));
		return -1;
	}
}

int
bt_device_detach(struct bt_device *btdev)
{
	struct bt_interface *iface;
	void *buf;
	size_t n;
	int err;

	if ((btdev->flags & BTDF_ATTACH) ||
	    !(btdev->flags & BTDF_ATTACH_DONE))
		return 0;

	if (!(btdev->flags & BTDF_DEVINFO_VALID)) {
		if (bt_get_devinfo(btdev) < 0)
			return -1;
		if (!(btdev->flags & BTDF_DEVINFO_VALID))
			return 0;
	}

	if (bdaddr_any(&btdev->info.baa.bd_laddr)) {
		if ((iface = bt_find_attach_interface(btdev)) == NULL)
			return 0;

		bdaddr_copy(&btdev->info.baa.bd_laddr, &iface->addr);
	}

	if (devinfo_store(&btdev->info, &buf, &n) < 0)
		return -1;

	bt_priv_msg(IMSG_DETACH);
	bt_priv_send(&n, sizeof(n));
	bt_priv_send(buf, n);
	bt_priv_recv(&err, sizeof(int));
	free(buf);

	switch (err) {
	case 0:
	case ENODEV:
		btdev->flags &= ~BTDF_ATTACH_DONE;
		return 0;
	default:
		log_warnx("could not detach %s (%s)",
		    bt_ntoa(&btdev->addr, NULL), strerror(err));
		return -1;
	}
}
