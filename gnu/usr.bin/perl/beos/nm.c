/* nm.c - a feeble shared-lib library parser
 * Copyright 1997, 1998 Tom Spindler
 * This software is covered under perl's Artistic license.
 */

/* $Id: nm.c,v 1.1 1998/02/16 03:51:26 dogcow Exp $ */

#include <be/kernel/image.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

main(int argc, char **argv) {
char *path, *symname;
image_id img;
int32 n = 0;
volatile int32 symnamelen, symtype;
void *symloc;

if (argc != 2) { printf("more args, bozo\n"); exit(1); }

path = (void *) malloc((size_t) 2048);
symname = (void *) malloc((size_t) 2048);

if (!getcwd(path, 2048)) { printf("aiee!\n"); exit(1); }
if (!strcat(path, "/"))  {printf("naah.\n"); exit (1); }
/*printf("%s\n",path);*/

if ('/' != argv[1][0]) {
	if (!strcat(path, argv[1])) { printf("feh1\n"); exit(1); }
} else {
	if (!strcpy(path, argv[1])) { printf("gah!\n"); exit(1); }
}
/*printf("%s\n",path);*/

img = load_add_on(path);
if (B_ERROR == img) {printf("Couldn't load_add_on() %s.\n", path); exit(2); }

symnamelen=2047;

while (B_BAD_INDEX != get_nth_image_symbol(img, n++, symname, &symnamelen,
                        &symtype, &symloc)) {
	printf("%s |%s |GLOB %Lx | \n", symname,
         ((B_SYMBOL_TYPE_ANY == symtype) || (B_SYMBOL_TYPE_TEXT == symtype)) ? "FUNC" : "VAR ", symloc);
	symnamelen=2047;
}
printf("number of symbols: %d\n", n);
if (B_ERROR == unload_add_on(img)) {printf("err while closing.\n"); exit(3); }
free(path);
return(0);
}
