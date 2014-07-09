/* $LynxId: LYMap.h,v 1.12 2010/09/25 11:35:42 tom Exp $ */
#ifndef LYMAP_H
#define LYMAP_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#include <HTList.h>
#include <HTAnchor.h>

#ifdef __cplusplus
extern "C" {
#endif
    extern BOOL LYMapsOnly;

    extern void ImageMapList_free(HTList *list);
    extern void LYPrintImgMaps(FILE *fp);
    extern BOOL LYAddImageMap(char *address, char *title,
			      HTParentAnchor *node_anchor);
    extern BOOL LYAddMapElement(char *map, char *address, char *title,
				HTParentAnchor *node_anchor,
				int intern_flag);
    extern BOOL LYHaveImageMap(char *address);

#ifdef __cplusplus
}
#endif
#endif				/* LYMAP_H */
