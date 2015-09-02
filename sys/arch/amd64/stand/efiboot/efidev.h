void		 efid_init(struct diskinfo *, void *handle);
const char	*efi_getdisklabel(efi_diskinfo_t, struct disklabel *);
int		 efiopen(struct open_file *, ...);
int		 efistrategy(void *, int, daddr32_t, size_t, void *, size_t *);
int		 eficlose(struct open_file *);
int		 efiioctl(struct open_file *, u_long, void *);

