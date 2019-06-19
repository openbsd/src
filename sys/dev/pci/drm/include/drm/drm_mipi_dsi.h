/* Public domain. */

#ifndef _DEV_PCI_DRM_DRM_MIPI_DSI_H_
#define _DEV_PCI_DRM_DRM_MIPI_DSI_H_

#include <sys/types.h>
#include <linux/errno.h>
#include <linux/types.h>

struct mipi_dsi_host;
struct mipi_dsi_device;
struct mipi_dsi_msg;

struct mipi_dsi_host_ops {
	int (*attach)(struct mipi_dsi_host *, struct mipi_dsi_device *);
	int (*detach)(struct mipi_dsi_host *, struct mipi_dsi_device *);
	ssize_t (*transfer)(struct mipi_dsi_host *, const struct mipi_dsi_msg *);
};

struct mipi_dsi_host {
	const struct mipi_dsi_host_ops *ops;
};

struct mipi_dsi_device {
	struct mipi_dsi_host *host;
	uint32_t channel;
	uint32_t mode_flags;
#define MIPI_DSI_MODE_LPM	(1 << 0)
};

struct mipi_dsi_msg {
	uint8_t type;
	uint8_t channel;
	uint16_t flags;
#define MIPI_DSI_MSG_USE_LPM	(1 << 0)
	const uint8_t *tx_buf;
	size_t tx_len;
	uint8_t *rx_buf;
	size_t rx_len;
};

struct mipi_dsi_packet {
	size_t size;
	size_t payload_length;
	uint8_t	header[4];
	const uint8_t *payload;
};

enum mipi_dsi_dcs_tear_mode {
	MIPI_DSI_DCS_TEAR_MODE_UNUSED
};

enum mipi_dsi_pixel_format {
	MIPI_DSI_FMT_RGB888,
	MIPI_DSI_FMT_RGB666,
	MIPI_DSI_FMT_RGB666_PACKED,
	MIPI_DSI_FMT_RGB565,
};

int mipi_dsi_attach(struct mipi_dsi_device *);
int mipi_dsi_create_packet(struct mipi_dsi_packet *,
    const struct mipi_dsi_msg *);
ssize_t mipi_dsi_generic_write(struct mipi_dsi_device *, const void *, size_t);
ssize_t mipi_dsi_dcs_write_buffer(struct mipi_dsi_device *, const void *,
    size_t);
ssize_t mipi_dsi_dcs_read(struct mipi_dsi_device *, u8, void *, size_t);
ssize_t mipi_dsi_dcs_write(struct mipi_dsi_device *, u8, const void *, size_t);

static inline int
mipi_dsi_pixel_format_to_bpp(enum mipi_dsi_pixel_format fmt)
{
	switch (fmt) {
	case MIPI_DSI_FMT_RGB888:
	case MIPI_DSI_FMT_RGB666:
		return 24;
	case MIPI_DSI_FMT_RGB666_PACKED:
		return 18;
	case MIPI_DSI_FMT_RGB565:
		return 16;
	}
	return -EINVAL;
}

#endif
