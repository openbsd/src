
#ifndef LYMAP_H
#define LYMAP_H

extern BOOL LYMapsOnly;

extern void ImageMapList_free PARAMS((HTList * list));
extern BOOL LYAddImageMap PARAMS((char *address, char *title,
				  HTParentAnchor *node_anchor));
extern BOOL LYAddMapElement PARAMS((char *map, char *address, char *title,
				    HTParentAnchor *node_anchor,
				    BOOL intern_flag));
extern BOOL LYHaveImageMap PARAMS((char *address));

#endif /* LYMAP_H */
