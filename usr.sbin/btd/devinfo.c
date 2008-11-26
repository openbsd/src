#include <stdlib.h>
#include <string.h>

#include "btd.h"

bdaddr_t *
devinfo_laddr(struct bt_devinfo *info)
{
	return &info->baa.bd_laddr;
}

bdaddr_t *
devinfo_raddr(struct bt_devinfo *info)
{
	return &info->baa.bd_raddr;
}

int
devinfo_store(const struct bt_devinfo *info, void **data, size_t *datalen)
{
	*datalen = sizeof(info->baa);
	if (info->baa.bd_type == BTDEV_HID)
		*datalen += info->baa.bd_hid.hid_dlen;

	if ((*data = malloc(*datalen)) == NULL) {
		log_warn("devinfo_store");
		return -1;
	}

	memcpy(*data, &info->baa, sizeof(info->baa));
	if (info->baa.bd_type == BTDEV_HID &&
	    info->baa.bd_hid.hid_dlen > 0)
		memcpy((uint8_t *)*data + sizeof(info->baa),
		    info->baa.bd_hid.hid_desc,
		    info->baa.bd_hid.hid_dlen);

	return 0;
}

int
devinfo_load(struct bt_devinfo *info, void *data, size_t datalen)
{
	return devinfo_load_attach_args(&info->baa, data, datalen);
}

void
devinfo_unload(struct bt_devinfo *info)
{
	return devinfo_unload_attach_args(&info->baa);
}

int
devinfo_load_attach_args(struct btdev_attach_args *baa, void *data,
    size_t datalen)
{
	if (datalen < sizeof(*baa)) {
		log_warnx("devinfo data too short");
		memset(baa, 0, sizeof(*baa));
		return -1;
	}

	memcpy(baa, data, sizeof(*baa));
	data = (struct btdev_attach_args *)data + 1;
	datalen -= sizeof(*baa);

	if (baa->bd_type == BTDEV_HID) {
		uint16_t dlen = baa->bd_hid.hid_dlen;
		void *desc = NULL;

		if (datalen != dlen) {
			log_warnx("bad devinfo data length (HID)");
			return -1;
		}

		if (dlen > 0) {
			if ((desc = malloc(dlen)) == NULL) {
				log_warn("devinfo_load_attach_args");
				return -1;
			}
			memcpy(desc, data, dlen);
		}

		baa->bd_hid.hid_desc = desc;
	} else if (datalen > 0) {
		log_warnx("devinfo data too long");
		return -1;
	}

	return 0;
}

void
devinfo_unload_attach_args(struct btdev_attach_args *baa)
{
	if (baa->bd_type == BTDEV_HID && baa->bd_hid.hid_desc != NULL) {
		free(baa->bd_hid.hid_desc);
		baa->bd_hid.hid_desc = NULL;
		baa->bd_hid.hid_dlen = 0;
	}
}

void
devinfo_dump(const struct bt_devinfo *info)
{
	const struct btdev_attach_args *baa = &info->baa;

	log_info("laddr %s", bt_ntoa(&baa->bd_laddr, NULL));
	log_info("raddr %s", bt_ntoa(&baa->bd_raddr, NULL));
	log_info("type %#x", baa->bd_type);
	log_info("mode %d", baa->bd_mode);
}
