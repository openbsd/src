#define ROM_VECTORS	0xff000000

#define ROM_VERSION	(ROM_VECTORS + 0)
#define ROM_COMM_AREA	(ROM_VECTORS + 8)
#define ROM_COMMAND	(ROM_VECTORS + 16)
#define ROM_SLAVEHALT	(ROM_VECTORS + 24)
#define ROM_MSGBUFP	(ROM_VECTORS + 32)
#define ROM_DGRAM	(ROM_VECTORS + 40)
#define ROM_EEVERSION	(ROM_VECTORS + 48)
#define ROM_REVISION	(ROM_VECTORS + 56)

struct prom_command_area
{
  char *command_ptr;
  int ret_val;
  int first_free;
  int memsize;
  int ramdisk;
  char *iomap_addr;
  int (*slave_start)();
  int row;
  int col;
  int silent;
};
