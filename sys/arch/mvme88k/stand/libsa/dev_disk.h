
int	disk_open __P((struct open_file *, ...));
int	disk_close __P((struct open_file *));
int	disk_strategy __P((void *, int, daddr_t, u_int, char *, u_int *));
int	disk_ioctl();

