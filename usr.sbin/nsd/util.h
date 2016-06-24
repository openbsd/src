/*
 * util.h -- set of various support routines.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#ifndef _UTIL_H_
#define _UTIL_H_

#include <sys/time.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
struct rr;
struct buffer;
struct region;

#ifdef HAVE_SYSLOG_H
#  include <syslog.h>
#else
#  define LOG_ERR 3
#  define LOG_WARNING 4
#  define LOG_NOTICE 5
#  define LOG_INFO 6
#endif

#define ALIGN_UP(n, alignment)  \
	(((n) + (alignment) - 1) & (~((alignment) - 1)))
#define PADDING(n, alignment)   \
	(ALIGN_UP((n), (alignment)) - (n))

/*
 * Initialize the logging system.  All messages are logged to stderr
 * until log_open and log_set_log_function are called.
 */
void log_init(const char *ident);

/*
 * Open the system log.  If FILENAME is not NULL, a log file is opened
 * as well.
 */
void log_open(int option, int facility, const char *filename);

/*
 * Reopen the logfile.
 */
void log_reopen(const char *filename, uint8_t verbose);

/*
 * Finalize the logging system.
 */
void log_finalize(void);

/*
 * Type of function to use for the actual logging.
 */
typedef void log_function_type(int priority, const char *message);

/*
 * The function used to log to the log file.
 */
log_function_type log_file;

/*
 * The function used to log to syslog.  The messages are also logged
 * using log_file.
 */
log_function_type log_syslog;

/*
 * Set the logging function to use (log_file or log_syslog).
 */
void log_set_log_function(log_function_type *log_function);

/*
 * Log a message using the current log function.
 */
void log_msg(int priority, const char *format, ...)
	ATTR_FORMAT(printf, 2, 3);

/*
 * Log a message using the current log function.
 */
void log_vmsg(int priority, const char *format, va_list args);

/*
 * Verbose output switch
 */
extern int verbosity;
#define VERBOSITY(level, args)					\
	do {							\
		if ((level) <= verbosity) {			\
			log_msg args ;				\
		}						\
	} while (0)

/*
 * Set the INDEXth bit of BITS to 1.
 */
void set_bit(uint8_t bits[], size_t index);

/*
 * Set the INDEXth bit of BITS to 0.
 */
void clear_bit(uint8_t bits[], size_t index);

/*
 * Return the value of the INDEXth bit of BITS.
 */
int get_bit(uint8_t bits[], size_t index);

/* A general purpose lookup table */
typedef struct lookup_table lookup_table_type;
struct lookup_table {
	int id;
	const char *name;
};

/*
 * Looks up the table entry by name, returns NULL if not found.
 */
lookup_table_type *lookup_by_name(lookup_table_type table[], const char *name);

/*
 * Looks up the table entry by id, returns NULL if not found.
 */
lookup_table_type *lookup_by_id(lookup_table_type table[], int id);

/*
 * (Re-)allocate SIZE bytes of memory.  Report an error if the memory
 * could not be allocated and exit the program.  These functions never
 * return NULL.
 */
void *xalloc(size_t size);
void *xmallocarray(size_t num, size_t size);
void *xalloc_zero(size_t size);
void *xalloc_array_zero(size_t num, size_t size);
void *xrealloc(void *ptr, size_t size);

/*
 * Mmap allocator routines.
 *
 */
#ifdef USE_MMAP_ALLOC
void *mmap_alloc(size_t size);
void mmap_free(void *ptr);
#endif /* USE_MMAP_ALLOC */

/*
 * Write SIZE bytes of DATA to FILE.  Report an error on failure.
 *
 * Returns 0 on failure, 1 on success.
 */
int write_data(FILE *file, const void *data, size_t size);

/*
 * like write_data, but keeps track of crc
 */
int write_data_crc(FILE *file, const void *data, size_t size, uint32_t* crc);

/*
 * Write the complete buffer to the socket, irrespective of short
 * writes or interrupts. This function blocks to write the data.
 * Returns 0 on error, 1 on success.
 */
int write_socket(int s, const void *data, size_t size);

/*
 * Copy data allowing for unaligned accesses in network byte order
 * (big endian).
 */
static inline void
write_uint16(void *dst, uint16_t data)
{
#ifdef ALLOW_UNALIGNED_ACCESSES
	* (uint16_t *) dst = htons(data);
#else
	uint8_t *p = (uint8_t *) dst;
	p[0] = (uint8_t) ((data >> 8) & 0xff);
	p[1] = (uint8_t) (data & 0xff);
#endif
}

static inline void
write_uint32(void *dst, uint32_t data)
{
#ifdef ALLOW_UNALIGNED_ACCESSES
	* (uint32_t *) dst = htonl(data);
#else
	uint8_t *p = (uint8_t *) dst;
	p[0] = (uint8_t) ((data >> 24) & 0xff);
	p[1] = (uint8_t) ((data >> 16) & 0xff);
	p[2] = (uint8_t) ((data >> 8) & 0xff);
	p[3] = (uint8_t) (data & 0xff);
#endif
}

/*
 * Copy data allowing for unaligned accesses in network byte order
 * (big endian).
 */
static inline uint16_t
read_uint16(const void *src)
{
#ifdef ALLOW_UNALIGNED_ACCESSES
	return ntohs(* (uint16_t *) src);
#else
	uint8_t *p = (uint8_t *) src;
	return (p[0] << 8) | p[1];
#endif
}

static inline uint32_t
read_uint32(const void *src)
{
#ifdef ALLOW_UNALIGNED_ACCESSES
	return ntohl(* (uint32_t *) src);
#else
	uint8_t *p = (uint8_t *) src;
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
#endif
}

/*
 * Print debugging information using log_msg,
 * set the logfile as /dev/stdout or /dev/stderr if you like.
 * nsd -F 0xFFFF enables all debug facilities.
 */
#define DEBUG_PARSER           0x0001U
#define DEBUG_ZONEC            0x0002U
#define DEBUG_QUERY            0x0004U
#define DEBUG_DBACCESS         0x0008U
#define DEBUG_NAME_COMPRESSION 0x0010U
#define DEBUG_XFRD             0x0020U
#define DEBUG_IPC              0x0040U

extern unsigned nsd_debug_facilities;
extern int nsd_debug_level;
#ifdef NDEBUG
#define DEBUG(facility, level, args)  /* empty */
#else
#define DEBUG(facility, level, args)				\
	do {							\
		if ((facility) & nsd_debug_facilities &&	\
		    (level) <= nsd_debug_level) {		\
			log_msg args ;				\
		}						\
	} while (0)
#endif

/* set to true to log time prettyprinted, or false to print epoch */
extern int log_time_asc;

/*
 * Timespec functions.
 */
int timespec_compare(const struct timespec *left, const struct timespec *right);
void timespec_add(struct timespec *left, const struct timespec *right);
void timespec_subtract(struct timespec *left, const struct timespec *right);

static inline void
timeval_to_timespec(struct timespec *left,
		    const struct timeval *right)
{
	left->tv_sec = right->tv_sec;
	left->tv_nsec = 1000 * right->tv_usec;
}

/* get the time */
void get_time(struct timespec* t);

/*
 * Converts a string representation of a period of time into
 * a long integer of seconds or serial value.
 *
 * Set the endptr to the first illegal character.
 *
 * Interface is similar as strtol(3)
 *
 * Returns:
 *	LONG_MIN if underflow occurs
 *	LONG_MAX if overflow occurs.
 *	otherwise number of seconds
 *
 * XXX These functions do not check the range.
 *
 */
uint32_t strtoserial(const char *nptr, const char **endptr);
uint32_t strtottl(const char *nptr, const char **endptr);

/*
 * Convert binary data to a string of hexadecimal characters.
 */
ssize_t hex_ntop(uint8_t const *src, size_t srclength, char *target,
		 size_t targsize);
ssize_t hex_pton(const char* src, uint8_t* target, size_t targsize);

/*
 * convert base32 data from and to string. Returns length.
 * -1 on error. Use (byte count*8)%5==0.
 */
int b32_pton(char const *src, uint8_t *target, size_t targsize);
int b32_ntop(uint8_t const *src, size_t srclength, char *target,
	size_t targsize);

/*
 * Strip trailing and leading whitespace from str.
 */
void strip_string(char *str);

/*
 * Convert a single (hexadecimal) digit to its integer value.
 */
int hexdigit_to_int(char ch);

/*
 * Convert TM to seconds since epoch (midnight, January 1st, 1970).
 * Like timegm(3), which is not always available.
 */
time_t mktime_from_utc(const struct tm *tm);

/*
 * Add bytes to given crc. Returns new CRC sum.
 * Start crc val with 0xffffffff on first call. XOR crc with
 * 0xffffffff at the end again to get final POSIX 1003.2 checksum.
 */
uint32_t compute_crc(uint32_t crc, uint8_t* data, size_t len);

/*
 * Compares two 32-bit serial numbers as defined in RFC1982.  Returns
 * <0 if a < b, 0 if a == b, and >0 if a > b.  The result is undefined
 * if a != b but neither is greater or smaller (see RFC1982 section
 * 3.2.).
 */
int compare_serial(uint32_t a, uint32_t b);

/*
 * Generate a random query ID.
 */
uint16_t qid_generate(void);
/* value between 0 .. (max-1) inclusive */
int random_generate(int max);

/*
 * call region_destroy on (region*)data, useful for region_add_cleanup().
 */
void cleanup_region(void *data);

/*
 * Region used to store owner and origin of previous RR (used
 * for pretty printing of zone data).
 * Keep the same between calls to print_rr.
 */
struct state_pretty_rr {
	struct region *previous_owner_region;
	const struct dname *previous_owner;
	const struct dname *previous_owner_origin;
};
struct state_pretty_rr* create_pretty_rr(struct region* region);
/* print rr to file, returns 0 on failure(nothing is written) */
int print_rr(FILE *out, struct state_pretty_rr* state, struct rr *record,
	struct region* tmp_region, struct buffer* tmp_buffer); 

/*
 * Convert a numeric rcode value to a human readable string
 */
const char* rcode2str(int rc);

void addr2str(
#ifdef INET6
	struct sockaddr_storage *addr
#else
	struct sockaddr_in *addr
#endif
	, char* str, size_t len);

#endif /* _UTIL_H_ */
