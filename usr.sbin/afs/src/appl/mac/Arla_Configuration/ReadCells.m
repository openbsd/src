#import <Cocoa/Cocoa.h>
#import "ReadCells.h"
#include <stdio.h>
#include <string.h>

#define CELLSERVDB "/usr/arla/etc/CellServDB"
#define THESECELLS "/usr/arla/etc/TheseCells"
#define THISCELL "/usr/arla/etc/ThisCell"
#define DYNROOTDB "/usr/arla/etc/DynRootDB"

static char *
getcell (char *line)
{
     int len;

     len = strcspn (line, " \t#");
     line[len] = '\0';
     return line;
}

@implementation ReadCells

+ (int)
auth: (NSMutableArray *) authArray
show: (NSMutableArray *) showArray
cell: (NSMutableArray *) cellArray {
    FILE *f;
    char line[256];
    int lineno = 0;
    char *cell;
    int arrayindex;
    
    f = fopen (CELLSERVDB, "r");
    if (f != NULL) {
	while (fgets (line, sizeof (line), f)) {
	    ++lineno;
	    line[strlen(line) - 1] = '\0';
	    if (line[0] == '#' || line[0] == '\0')
		continue;
	    if (line[0] == '>') {
		cell = getcell (&line[1]);
		[ authArray addObject: [[NSNumber alloc] initWithInt: NSOffState] ];
		[ showArray addObject: [[NSNumber alloc] initWithInt: NSOffState] ];
		[ cellArray addObject: [[NSString alloc] initWithCString: cell] ];
	    }
	}
	fclose (f);
    }

    f = fopen(THESECELLS, "r");
    if (f != NULL) {
	while (fgets (line, sizeof (line), f)) {
	    ++lineno;
	    line[strlen(line) - 1] = '\0';
	    arrayindex = [cellArray indexOfObject: [[NSString alloc] initWithCString: line]];
	    if (arrayindex == NSNotFound) {
		[ authArray addObject: [[NSNumber alloc] initWithInt: NSOnState] ];
		[ showArray addObject: [[NSNumber alloc] initWithInt: NSOffState] ];
		[ cellArray addObject: [[NSString alloc] initWithCString: line] ];    
	    } else {
		[ authArray replaceObjectAtIndex: arrayindex
			    withObject: [[NSNumber alloc] initWithInt: NSOnState]];
	    }
	}
	fclose (f);
    }

    f = fopen(DYNROOTDB, "r");
    if (f != NULL) {
	while (fgets (line, sizeof (line), f)) {
	    ++lineno;
	    line[strlen(line) - 1] = '\0';
	    arrayindex = [cellArray indexOfObject: [[NSString alloc] initWithCString: line]];
	    if (arrayindex == NSNotFound) {
		[ authArray addObject: [[NSNumber alloc] initWithInt: NSOffState] ];
		[ showArray addObject: [[NSNumber alloc] initWithInt: NSOnState] ];
		[ cellArray addObject: [[NSString alloc] initWithCString: line] ];    
	    } else {
		[ showArray replaceObjectAtIndex: arrayindex
			    withObject: [[NSNumber alloc] initWithInt: NSOnState]];
	    }
	}
	fclose (f);
    }

    return 0;
}
@end
