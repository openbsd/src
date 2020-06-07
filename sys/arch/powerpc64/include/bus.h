#ifndef _MACHINE_BUS_H_
#define _MACHINE_BUS_H_

struct bus_space;
typedef u_long bus_addr_t;
typedef u_long bus_size_t;
typedef u_long bus_space_handle_t;
typedef struct bus_space *bus_space_tag_t;

#define BUS_SPACE_MAP_PREFETCHABLE	0x01

#define bus_space_map(t, o, s, c, p)	ENOMEM
#define bus_space_write_4(t, h, o, v)
#define bus_space_read_4(t, h, o) 0xffffffff

struct bus_dma_tag;
typedef struct bus_dma_tag *bus_dma_tag_t;

#endif /* _MACHINE_BUS_H_ */
