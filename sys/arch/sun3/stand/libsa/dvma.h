
void dvma_init();

char * dvma_mapin(char *pkt, int len);
void dvma_mapout(char *dmabuf, int len);

char * dvma_alloc(int len);
void dvma_free(char *dvma, int len);

