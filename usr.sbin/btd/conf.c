#include <sys/types.h>

#include <netbt/hci.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "btd.h"

struct btd *
conf_new(void)
{
	struct btd *conf;

	if ((conf = calloc(1, sizeof(*conf))) == NULL)
		return NULL;

	TAILQ_INIT(&conf->interfaces);
	TAILQ_INIT(&conf->devices);

	return conf;
}

void
conf_delete(struct btd *conf)
{
	while (!TAILQ_EMPTY(&conf->interfaces))
		conf_delete_interface(TAILQ_FIRST(&conf->interfaces));

	while (!TAILQ_EMPTY(&conf->devices))
		conf_delete_device(TAILQ_FIRST(&conf->devices));
}

/* cope well, and silently with bad arguments, because this function
   may be called via the control socket */
struct bt_interface *
conf_add_interface(struct btd *conf, const bdaddr_t *addr)
{
	struct bt_interface *iface;

	if (conf_find_interface(conf, addr) != NULL) {
		errno = EEXIST;
		return NULL;
	}

	if ((iface = calloc(1, sizeof(*iface))) == NULL)
		return NULL;

	iface->env = conf;
	bdaddr_copy(&iface->addr, addr);

	TAILQ_INSERT_TAIL(&conf->interfaces, iface, entry);

	return iface;
}

struct bt_interface *
conf_find_interface(const struct btd *conf, const bdaddr_t *addr)
{
	struct bt_interface *iface;

	TAILQ_FOREACH(iface, &conf->interfaces, entry) {
		if (bdaddr_same(&iface->addr, addr))
			return iface;
	}

	return NULL;
}

void
conf_delete_interface(struct bt_interface *iface)
{
	struct btd *conf = iface->env;

	TAILQ_REMOVE(&conf->interfaces, iface, entry);

	free(iface);
}

/* cope well, and silently with bad arguments, because this function
   may be called via the control socket */
struct bt_device *
conf_add_device(struct btd *conf, const bdaddr_t *addr)
{
	struct bt_device *btdev;

	if (conf_find_device(conf, addr) != NULL) {
		errno = EEXIST;
		return NULL;
	}

	if ((btdev = calloc(1, sizeof(*btdev))) == NULL)
		return NULL;

	btdev->env = conf;
	bdaddr_copy(&btdev->addr, addr);

	TAILQ_INSERT_TAIL(&conf->devices, btdev, entry);

	return btdev;
}

struct bt_device *
conf_find_device(const struct btd *conf, const bdaddr_t *addr)
{
	struct bt_device *btdev;

	TAILQ_FOREACH(btdev, &conf->devices, entry) {
		if (bdaddr_same(&btdev->addr, addr))
			return btdev;
	}

	return NULL;
}

void
conf_delete_device(struct bt_device *btdev)
{
	struct btd *conf = btdev->env;

	TAILQ_REMOVE(&conf->devices, btdev, entry);

	free(btdev);
}

void
conf_lookup_pin(const struct btd *conf, const bdaddr_t *addr,
    uint8_t pin[HCI_PIN_SIZE], uint8_t *pin_size)
{
	struct bt_device *btdev;

	if ((btdev = conf_find_device(conf, addr)) == NULL &&
	    (btdev = conf_find_device(conf, BDADDR_ANY)) == NULL) {
		memset(pin, 0, HCI_PIN_SIZE);
		*pin_size = 0;
		return;
	}

	memcpy(pin, btdev->pin, HCI_PIN_SIZE);
	*pin_size = btdev->pin_size;
}

void
conf_dump(const struct btd *conf)
{
	struct bt_interface *iface;
	struct bt_device *btdev;

	TAILQ_FOREACH(iface, &conf->interfaces, entry) {
		log_debug("interface %s%s", bt_ntoa(&iface->addr, NULL),
		    iface->disabled ? " disabled" : "");
	}

	TAILQ_FOREACH(btdev, &conf->devices, entry) {
		log_debug("%s %s type %#x%s%*s%s (%s)",
		    btdev->flags & BTDF_ATTACH ? "attach" : "device",
		    bt_ntoa(&btdev->addr, NULL), btdev->type,
		    btdev->pin_size > 0 ? " pin \"" : "", btdev->pin_size,
		    btdev->pin, btdev->pin_size > 0 ? "\"" : "",
		    btdev->flags & BTDF_ATTACH_DONE ? "attached" :
		    "detached");
	}

}
