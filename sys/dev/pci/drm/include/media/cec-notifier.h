/* Public domain. */

#ifndef _MEDIA_CEC_NOTIFIER_H
#define _MEDIA_CEC_NOTIFIER_H

struct cec_connector_info {
};

#define cec_notifier_set_phys_addr_from_edid(x, y)
#define cec_notifier_phys_addr_invalidate(x)
#define cec_notifier_put(x)
#define cec_notifier_get_conn(x, y)		NULL
#define cec_fill_conn_info_from_drm(x, y)
#define cec_notifier_conn_register(x, y, z)	(void *)1
#define cec_notifier_conn_unregister(x)

#endif
