/*
 * This file specifies the format of gmon.out files.  It should have
 * as few external dependencies as possible as it is going to be
 * included in many different programs.  That is, minimize the
 * number of #include's.
 *
 * A gmon.out file consists of a header (defined by gmon_hdr) followed
 * by a sequence of records.  Each record starts with a one-byte tag
 * identifying the type of records, followed by records specific data.
 */
#ifndef gmon_out_h
#define gmon_out_h

#define	GMON_MAGIC	"gmon"	/* magic cookie */
#define GMON_VERSION	1	/* version number */

/*
 * Raw header as it appears on file (without padding):
 */
struct gmon_hdr
  {
    char cookie[4];
    char version[4];
    char spare[3 * 4];
  };

/* types of records in this file: */
typedef enum
  {
    GMON_TAG_TIME_HIST = 0, GMON_TAG_CG_ARC = 1, GMON_TAG_BB_COUNT = 2
  }
GMON_Record_Tag;

struct gmon_hist_hdr
  {
    char low_pc[sizeof (char*)];	/* base pc address of sample buffer */
    char high_pc[sizeof (char*)];	/* max pc address of sampled buffer */
    char hist_size[4];			/* size of sample buffer */
    char prof_rate[4];			/* profiling clock rate */
    char dimen[15];			/* phys. dim., usually "seconds" */
    char dimen_abbrev;			/* usually 's' for "seconds" */
  };

struct gmon_cg_arc_record
  {
    char from_pc[sizeof (char*)];	/* address within caller's body */
    char self_pc[sizeof (char*)];	/* address within callee's body */
    char count[4];			/* number of arc traversals */
  };

#endif /* gmon_out_h */
