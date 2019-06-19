/* Public domain. */

#ifndef _LINUX_PM_QOS_H
#define _LINUX_PM_QOS_H

struct pm_qos_request {
};

#define PM_QOS_CPU_DMA_LATENCY	1

#define PM_QOS_DEFAULT_VALUE	-1

#define pm_qos_update_request(a, b)
#define pm_qos_add_request(a, b, c)
#define pm_qos_remove_request(a)

#endif
