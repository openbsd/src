/*	$OpenBSD: btd.h,v 1.1 2008/11/24 23:34:42 uwe Exp $	*/

/*
 * Copyright (c) 2008 Uwe Stuehler <uwe@openbsd.org>
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
#include <sys/queue.h>

#include <dev/bluetooth/btdev.h>
#include <netbt/bluetooth.h>

#include <db.h>
#include <event.h>
#include <pwd.h>
#include <stdarg.h>

#define DEFAULT_INTERFACE_NAME "OpenBSD"

#define BTD_DB		"/var/db/btd.db"
#define BTD_SOCKET	"/var/run/btd.sock"
#define BTD_USER	"_btd"

/* XXX there is no need for more than one bthub device! */
#define BTHUB_PATH	"/dev/bthub0"

#define	READ_BUF_SIZE		8192

struct btd;

struct bt_interface {
	TAILQ_ENTRY(bt_interface) entry;
	struct event ev;
	struct btd *env;
	const char *xname;
	bdaddr_t addr;
	const char *name;
	int disabled;
	int fd;
};

struct btd_db {
	DB *dbh;
};

struct btd_hci {
	struct bt_interface *inquiry_interface;
	int inquiry_running;
	time_t last_inquiry;	/* start time */
};

struct bt_devinfo {
	/* XXX don't tie it to a kernel structure */
	struct btdev_attach_args baa;
};

struct bt_device {
	TAILQ_ENTRY(bt_device) entry;
	bdaddr_t addr;
	uint16_t type;
	uint8_t *pin;		/* HCI_PIN_SIZE when not NULL */
	time_t last_seen;	/* last response to an inquiry */
	int flags;
	struct bt_devinfo info;
};

#define BTDF_VISIBLE		0x0001	/* device is discoverable */
#define BTDF_ATTACH		0x0002	/* attempt to attach a driver */

struct btd {
	int debug;
	struct btd_db db;
	struct btd_hci hci;
	TAILQ_HEAD(interfaces, bt_interface) interfaces;
	TAILQ_HEAD(devices, bt_device) devices;
};

/* ipc messages */

enum imsg_type {
	IMSG_NONE,
	IMSG_CONFIG_BEGIN,
	IMSG_CONFIG_INTERFACE,
	IMSG_CONFIG_DEVICE,
	IMSG_CONFIG_COMMIT,
	IMSG_CONFIG_ROLLBACK,
	IMSG_ATTACH,
	IMSG_DETACH
};

/* prototypes */

/* bt.c */
pid_t bt_main(int[2], struct btd *, struct passwd *);

/* conf.c */
struct bt_device *conf_add_device(struct btd *,
    const bdaddr_t *);
struct bt_device *conf_find_device(const struct btd *,
    const bdaddr_t *);
struct bt_interface *conf_add_interface(struct btd *,
    const bdaddr_t *);
struct bt_interface *conf_find_interface(const struct btd *,
    const bdaddr_t *);
const uint8_t *conf_lookup_pin(const struct btd *, const bdaddr_t *);

/* control.c */
void control_init(struct btd *);
void control_cleanup(void);

/* db.c */
void db_open(const char *, struct btd_db *);
int db_put_link_key(struct btd_db *, const bdaddr_t *, const uint8_t *);
int db_get_link_key(struct btd_db *, const bdaddr_t *, uint8_t *);
int db_put_devinfo(struct btd_db *, const bdaddr_t *,
    const struct bt_devinfo *);
int db_get_devinfo(struct btd_db *, const bdaddr_t *, struct bt_devinfo *);
void db_dump(struct btd_db *);

/* devinfo.c */
int devinfo_load(struct bt_devinfo *, void *, size_t);
void devinfo_unload(struct bt_devinfo *);
int devinfo_store(const struct bt_devinfo *, void **, size_t *);
int devinfo_load_attach_args(struct btdev_attach_args *, void *, size_t);
void devinfo_unload_attach_args(struct btdev_attach_args *);
void devinfo_dump(const struct bt_devinfo *);

/* hci.c */
int hci_init(struct btd *);
time_t hci_ping(struct btd *);
int hci_dispatch(int, struct btd *);

/* log.c */
extern int debug;
void log_init(int);
void vlog(int, const char *, va_list);
void log_warn(const char *, ...);
void log_warnx(const char *, ...);
void log_info(const char *, ...);
void log_debug(const char *, ...);
void log_packet(const bdaddr_t *, const bdaddr_t *, const char *, ...);
void fatal(const char *);
void fatalx(const char *);

/* sdp.c */
int sdp_query(struct btdev_attach_args *, bdaddr_t *, bdaddr_t *, const char *);

/* util.c */
time_t getmonotime(void);
