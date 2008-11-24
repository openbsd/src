#include <sys/types.h>

#include <netbt/hci.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "btd.h"

struct bt_device *
conf_add_device(struct btd *conf, const bdaddr_t *addr)
{
	struct bt_device *btdev;
	struct bt_device *defaults;

	assert(conf_find_device(conf, addr) == NULL);

	btdev = calloc(1, sizeof(*btdev));
	if (btdev == NULL) {
		log_warn("conf_add_device");
		return NULL;
	}

	bdaddr_copy(&btdev->addr, addr);

	defaults = bdaddr_any(addr) ? NULL :
	    conf_find_device(conf, BDADDR_ANY);

	if (defaults != NULL) {
		btdev->type = defaults->type;
		btdev->flags = defaults->flags;

		if (defaults->pin != NULL) {
			if ((btdev->pin = malloc(HCI_KEY_SIZE)) == NULL) {
				log_warn("conf_add_device malloc");
				TAILQ_REMOVE(&conf->devices, btdev, entry);
				return NULL;
			}
			memcpy(btdev->pin, defaults->pin, HCI_KEY_SIZE);
		}
	}

	TAILQ_INSERT_TAIL(&conf->devices, btdev, entry);

	return btdev;
}

struct bt_interface *
conf_add_interface(struct btd *conf, const bdaddr_t *addr)
{
	struct bt_interface *iface;
	struct bt_interface *defaults;

	assert(conf_find_interface(conf, addr) == NULL);

	iface = calloc(1, sizeof(*iface));
	if (iface == NULL) {
		log_warn("conf_add_interface");
		return NULL;
	}

	bdaddr_copy(&iface->addr, addr);
	iface->env = conf;
	iface->fd = -1;

	defaults = bdaddr_any(addr) ? NULL :
	    conf_find_interface(conf, BDADDR_ANY);

	if (defaults != NULL) {
		if (defaults->name != NULL &&
		    (iface->name = strdup(defaults->name)) == NULL) {
			log_warn("conf_add_interface strdup");
			TAILQ_REMOVE(&conf->interfaces, iface, entry);
			free(iface);
			return NULL;
		}

		iface->disabled = defaults->disabled;
	}

	TAILQ_INSERT_TAIL(&conf->interfaces, iface, entry);

	return iface;
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

const uint8_t *
conf_lookup_pin(const struct btd *conf, const bdaddr_t *addr)
{
	struct bt_device *btdev;

	if ((btdev = conf_find_device(conf, addr)) == NULL &&
	    (btdev = conf_find_device(conf, BDADDR_ANY)) == NULL)
		return NULL;

	return btdev->pin;
}
