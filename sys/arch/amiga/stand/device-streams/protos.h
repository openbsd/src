/* START: "getdevices.c" */
struct List * get_drive_list  (void);
void free_drive_list  (struct List *l);
int add_name_to_drive_list  (struct List *l, char *dev_name);
char * get_hard_drive_device_name  (struct DosList *dl);
ulong checksum  (ulong sl, ulong *buf);
void do_unit  (struct device *dev, struct device_data *dd);
void free_unit  (struct unit *u);
void get_partitions  (struct device_data *dd, struct unit *u);
void free_partition  (struct partition *p);
/* END: "getdevices.c" */
/* START: "devices.c" */
struct device_data * alloc_device  (char *name, ulong unit, ulong flags, ulong iosize);
void free_device  (struct device_data *dd);
int open_device  (struct device_data *dd);
void close_device  (struct device_data *dd);
ulong device_read  (struct device_data *dd, ulong offset, ulong bytes, void *buffer);
ulong device_write  (struct device_data *dd, ulong offset, ulong bytes, void *buffer);
int device_do_command  (struct device_data *dd, UWORD command);
/* END: "devices.c" */
/* START: "util.c" */
int string_to_number (char *s, unsigned long *num);
char * stripws  (char *s);
char *fgetline (FILE *fp);
int flush_to_eol (FILE *fp);
char *concat_strings (const char *before, const char *after);
void free_string (char *string);
char * alloc_string  (char *s);
int ask_bool  (int def, int other, char *f, ...);
void * zmalloc  (size_t b);
void zfree  (void *mem);
struct Node * find_name  (struct List *l, char *s);
void verbose_message  (char *f, ...);
void debug_message  (char *f, ...);
void verbose_debug_message  (char *f, ...);
void message  (char *f, ...);
void warn_message  (char *f, ...);
void vmessage  (char *f, va_list ap);
/* END: "util.c" */
/* START: "system" */
#include <clib/alib_protos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <pragmas/exec_pragmas.h>
#include <pragmas/dos_pragmas.h>
extern struct Library *DOSBase;
/* END: "system" */
