#define VOLOP_OPEN(type,part,volid,flags,vol) \
	((backstoretypes[(type)]->open)((part),(volid),(flags),(vol)))
#define VOLOP_FREE(vol) \
	((backstoretypes[(vol)->type]->free)(vol))
#define VOLOP_ICREATE(vol,ino,ntype,node) \
	((backstoretypes[(vol)->type]->icreate)((vol),(ino),(ntype),(node)))
#define VOLOP_IOPEN(vol,ino,flags,fd) \
	((backstoretypes[(vol)->type]->iopen)((vol),(ino),(flags),(fd)))
#define VOLOP_IUNLINK(vol,ino) \
	((backstoretypes[(vol)->type]->iunlink)(vol,ino))
#define VOLOP_REMOVE(vol) \
	((backstoretypes[(vol)->type]->remove)(vol))
#define VOLOP_TRUNCATE(vol,len) \
	((backstoretypes[(vol)->type]->truncate)(vol,len))


#define VLD_MAX_BACKSTORE_TYPES 10

extern vol_op *backstoretypes[VLD_MAX_BACKSTORE_TYPES];

int32_t dir_afs2local (int32_t vno);
int32_t dir_local2afs (int32_t vno);
int32_t file_afs2local (int32_t vno);
int32_t file_local2afs (int32_t vno);
