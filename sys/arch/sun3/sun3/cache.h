
/*
 * All sun3 cache implementations are write-back.
 * Flushes must be done before removing translations
 * from the MMU because the cache uses the MMU.
 */

extern int cache_size;

void cache_flush_page(vm_offset_t pgva);
void cache_flush_segment(vm_offset_t sgva);
void cache_flush_context(void);

