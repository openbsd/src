void *dlopen(char *path, int mode);
void *dlsym(void *handle, char *symbol);
char *dlerror(void);
int dlclose(void *handle);
