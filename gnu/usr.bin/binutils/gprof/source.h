#ifndef source_h
#define source_h

#include <stdio.h>
#include "gprof.h"
#include "search_list.h"

typedef struct source_file
  {
    struct source_file *next;
    const char *name;		/* name of source file */
    unsigned long ncalls;	/* # of "calls" to this file */
    int num_lines;		/* # of lines in file */
    int nalloced;		/* number of lines allocated */
    void **line;		/* usage-dependent per-line data */
  }
Source_File;

/*
 * Options:
 */
extern bool create_annotation_files;	/* create annotated output files? */

/*
 * List of directories to search for source files:
 */
extern Search_List src_search_list;

/*
 * Chain of source-file descriptors:
 */
extern Source_File *first_src_file;

/*
 * Returns pointer to source file descriptor for PATH/FILENAME.
 */
extern Source_File *source_file_lookup_path PARAMS ((const char *path));
extern Source_File *source_file_lookup_name PARAMS ((const char *filename));

/*
 * Read source file SF output annotated source.  The annotation is at
 * MAX_WIDTH characters wide and for each source-line an annotation is
 * obtained by invoking function ANNOTE.  ARG is an argument passed to
 * ANNOTE that is left uninterpreted by annotate_source().
 *
 * Returns a pointer to the output file (which maybe stdout) such
 * that summary statistics can be printed.  If the returned file
 * is not stdout, it should be closed when done with it.
 */
extern FILE *annotate_source PARAMS ((Source_File * sf, int max_width,
				      void (*annote) (char *b, int w, int l,
						      void *arg),
				      void *arg));

#endif /* source_h */
