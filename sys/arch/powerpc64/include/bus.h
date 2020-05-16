#ifndef _MACHINE_BUS_H_
#define _MACHINE_BUS_H_

struct bus_space;
typedef u_long bus_addr_t;
typedef u_long bus_size_t;
typedef u_long bus_space_handle_t;
typedef struct bus_space *bus_space_tag_t;

#define bus_space_write_4(t, h, o, v)
#define bus_space_read_4(t, h, o) 0xffffffff

#endif /* _MACHINE_BUS_H_ */
