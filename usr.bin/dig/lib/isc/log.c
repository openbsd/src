/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: log.c,v 1.17 2020/02/24 13:49:38 jsg Exp $ */

/*! \file
 * \author  Principal Authors: DCL */

#include <sys/time.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include <isc/log.h>
#include <isc/util.h>

/*
 * XXXDCL make dynamic?
 */
#define LOG_BUFFER_SIZE	(8 * 1024)

/*!
 * This is the structure that holds each named channel.  A simple linked
 * list chains all of the channels together, so an individual channel is
 * found by doing strcmp()s with the names down the list.  Their should
 * be no performance penalty from this as it is expected that the number
 * of named channels will be no more than a dozen or so, and name lookups
 * from the head of the list are only done when isc_log_usechannel() is
 * called, which should also be very infrequent.
 */
typedef struct isc_logchannel isc_logchannel_t;

struct isc_logchannel {
	char *				name;
	unsigned int			type;
	int 				level;
	unsigned int			flags;
	isc_logdestination_t 		destination;
	ISC_LINK(isc_logchannel_t)	link;
};

/*!
 * The logchannellist structure associates categories and modules with
 * channels.  First the appropriate channellist is found based on the
 * category, and then each structure in the linked list is checked for
 * a matching module.  It is expected that the number of channels
 * associated with any given category will be very short, no more than
 * three or four in the more unusual cases.
 */
typedef struct isc_logchannellist isc_logchannellist_t;

struct isc_logchannellist {
	const isc_logmodule_t *		module;
	isc_logchannel_t *		channel;
	ISC_LINK(isc_logchannellist_t)	link;
};

/*!
 * This structure is used to remember messages for pruning via
 * isc_log_[v]write1().
 */
typedef struct isc_logmessage isc_logmessage_t;

struct isc_logmessage {
	char *				text;
	struct timespec			time;
	ISC_LINK(isc_logmessage_t)	link;
};

/*!
 * The isc_logconfig structure is used to store the configurable information
 * about where messages are actually supposed to be sent -- the information
 * that could changed based on some configuration file, as opposed to the
 * the category/module specification of isc_log_[v]write[1] that is compiled
 * into a program, or the debug_level which is dynamic state information.
 */
struct isc_logconfig {
	isc_log_t *			lctx;
	ISC_LIST(isc_logchannel_t)	channels;
	ISC_LIST(isc_logchannellist_t) *channellists;
	unsigned int			channellist_count;
	unsigned int			duplicate_interval;
	int				highest_level;
	char *				tag;
	isc_boolean_t			dynamic;
};

/*!
 * This isc_log structure provides the context for the isc_log functions.
 * The log context locks itself in isc_log_doit, the internal backend to
 * isc_log_write.  The locking is necessary both to provide exclusive access
 * to the buffer into which the message is formatted and to guard against
 * competing threads trying to write to the same syslog resource.  (On
 * some systems, such as BSD/OS, stdio is thread safe but syslog is not.)
 * Unfortunately, the lock cannot guard against a _different_ logging
 * context in the same program competing for syslog's attention.  Thus
 * There Can Be Only One, but this is not enforced.
 * XXXDCL enforce it?
 *
 * Note that the category and module information is not locked.
 * This is because in the usual case, only one isc_log_t is ever created
 * in a program, and the category/module registration happens only once.
 * XXXDCL it might be wise to add more locking overall.
 */
struct isc_log {
	/* Not locked. */
	isc_logcategory_t *		categories;
	unsigned int			category_count;
	isc_logmodule_t *		modules;
	unsigned int			module_count;
	int				debug_level;
	/* Locked by isc_log lock. */
	isc_logconfig_t * 		logconfig;
	char 				buffer[LOG_BUFFER_SIZE];
	ISC_LIST(isc_logmessage_t)	messages;
};

/*!
 * Used when ISC_LOG_PRINTLEVEL is enabled for a channel.
 */
static const char *log_level_strings[] = {
	"debug",
	"info",
	"notice",
	"warning",
	"error",
	"critical"
};

/*!
 * Used to convert ISC_LOG_* priorities into syslog priorities.
 * XXXDCL This will need modification for NT.
 */
static const int syslog_map[] = {
	LOG_DEBUG,
	LOG_INFO,
	LOG_NOTICE,
	LOG_WARNING,
	LOG_ERR,
	LOG_CRIT
};

/*!
 * When adding new categories, a corresponding ISC_LOGCATEGORY_foo
 * definition needs to be added to <isc/log.h>.
 *
 * The default category is provided so that the internal default can
 * be overridden.  Since the default is always looked up as the first
 * channellist in the log context, it must come first in isc_categories[].
 */
isc_logcategory_t isc_categories[] = {
	{ "default", 0 },	/* "default" must come first. */
	{ "general", 0 },
	{ NULL, 0 }
};

/*!
 * See above comment for categories on LIBISC_EXTERNAL_DATA, and apply it to modules.
 */
isc_logmodule_t isc_modules[] = {
	{ "socket", 0 },
	{ "time", 0 },
	{ "interface", 0 },
	{ "timer", 0 },
	{ "file", 0 },
	{ "other", 0 },
	{ NULL, 0 }
};

/*!
 * This essentially constant structure must be filled in at run time,
 * because its channel member is pointed to a channel that is created
 * dynamically with isc_log_createchannel.
 */
static isc_logchannellist_t default_channel;

/*!
 * libisc logs to this context.
 */
isc_log_t *isc_lctx = NULL;

/*!
 * Forward declarations.
 */
static isc_result_t
assignchannel(isc_logconfig_t *lcfg, unsigned int category_id,
	      const isc_logmodule_t *module, isc_logchannel_t *channel);

static isc_result_t
sync_channellist(isc_logconfig_t *lcfg);

static void
isc_log_doit(isc_log_t *lctx, isc_logcategory_t *category,
	     isc_logmodule_t *module, int level, isc_boolean_t write_once,
	     const char *format, va_list args)
     __attribute__((__format__(__printf__, 6, 0)));

/*@{*/
/*!
 * Convenience macros.
 */

#define FACILITY(channel)	 (channel->destination.facility)
#define FILE_NAME(channel)	 (channel->destination.file.name)
#define FILE_STREAM(channel)	 (channel->destination.file.stream)
#define FILE_VERSIONS(channel)	 (channel->destination.file.versions)
#define FILE_MAXSIZE(channel)	 (channel->destination.file.maximum_size)

/*@}*/
/****
 **** Public interfaces.
 ****/

/*
 * Establish a new logging context, with default channels.
 */
isc_result_t
isc_log_create(isc_log_t **lctxp, isc_logconfig_t **lcfgp) {
	isc_log_t *lctx;
	isc_logconfig_t *lcfg = NULL;
	isc_result_t result;

	REQUIRE(lctxp != NULL && *lctxp == NULL);
	REQUIRE(lcfgp == NULL || *lcfgp == NULL);

	lctx = malloc(sizeof(*lctx));
	if (lctx != NULL) {
		lctx->categories = NULL;
		lctx->category_count = 0;
		lctx->modules = NULL;
		lctx->module_count = 0;
		lctx->debug_level = 0;

		ISC_LIST_INIT(lctx->messages);

		isc_log_registercategories(lctx, isc_categories);
		isc_log_registermodules(lctx, isc_modules);
		result = isc_logconfig_create(lctx, &lcfg);

	} else
		result = ISC_R_NOMEMORY;

	if (result == ISC_R_SUCCESS)
		result = sync_channellist(lcfg);

	if (result == ISC_R_SUCCESS) {
		lctx->logconfig = lcfg;

		*lctxp = lctx;
		if (lcfgp != NULL)
			*lcfgp = lcfg;

	} else {
		if (lcfg != NULL)
			isc_logconfig_destroy(&lcfg);
		if (lctx != NULL)
			isc_log_destroy(&lctx);
	}

	return (result);
}

isc_result_t
isc_logconfig_create(isc_log_t *lctx, isc_logconfig_t **lcfgp) {
	isc_logconfig_t *lcfg;
	isc_logdestination_t destination;
	isc_result_t result = ISC_R_SUCCESS;
	int level = ISC_LOG_INFO;

	REQUIRE(lcfgp != NULL && *lcfgp == NULL);

	lcfg = malloc(sizeof(*lcfg));

	if (lcfg != NULL) {
		lcfg->lctx = lctx;
		lcfg->channellists = NULL;
		lcfg->channellist_count = 0;
		lcfg->duplicate_interval = 0;
		lcfg->highest_level = level;
		lcfg->tag = NULL;
		lcfg->dynamic = ISC_FALSE;

		ISC_LIST_INIT(lcfg->channels);

	} else
		result = ISC_R_NOMEMORY;

	/*
	 * Create the default channels:
	 *   	default_syslog, default_stderr, default_debug and null.
	 */
	if (result == ISC_R_SUCCESS) {
		destination.facility = LOG_DAEMON;
		result = isc_log_createchannel(lcfg, "default_syslog",
					       ISC_LOG_TOSYSLOG, level,
					       &destination, 0);
	}

	if (result == ISC_R_SUCCESS) {
		destination.file.stream = stderr;
		destination.file.name = NULL;
		destination.file.versions = ISC_LOG_ROLLNEVER;
		destination.file.maximum_size = 0;
		result = isc_log_createchannel(lcfg, "default_stderr",
					       ISC_LOG_TOFILEDESC,
					       level,
					       &destination,
					       ISC_LOG_PRINTTIME);
	}

	if (result == ISC_R_SUCCESS) {
		/*
		 * Set the default category's channel to default_stderr,
		 * which is at the head of the channels list because it was
		 * just created.
		 */
		default_channel.channel = ISC_LIST_HEAD(lcfg->channels);

		destination.file.stream = stderr;
		destination.file.name = NULL;
		destination.file.versions = ISC_LOG_ROLLNEVER;
		destination.file.maximum_size = 0;
		result = isc_log_createchannel(lcfg, "default_debug",
					       ISC_LOG_TOFILEDESC,
					       ISC_LOG_DYNAMIC,
					       &destination,
					       ISC_LOG_PRINTTIME);
	}

	if (result == ISC_R_SUCCESS)
		result = isc_log_createchannel(lcfg, "null",
					       ISC_LOG_TONULL,
					       ISC_LOG_DYNAMIC,
					       NULL, 0);

	if (result == ISC_R_SUCCESS)
		*lcfgp = lcfg;

	else
		if (lcfg != NULL)
			isc_logconfig_destroy(&lcfg);

	return (result);
}

void
isc_log_destroy(isc_log_t **lctxp) {
	isc_log_t *lctx;
	isc_logconfig_t *lcfg;
	isc_logmessage_t *message;

	REQUIRE(lctxp != NULL);

	lctx = *lctxp;
	if (lctx->logconfig != NULL) {
		lcfg = lctx->logconfig;
		lctx->logconfig = NULL;
		isc_logconfig_destroy(&lcfg);
	}

	while ((message = ISC_LIST_HEAD(lctx->messages)) != NULL) {
		ISC_LIST_UNLINK(lctx->messages, message, link);

		free(message);
	}

	lctx->buffer[0] = '\0';
	lctx->debug_level = 0;
	lctx->categories = NULL;
	lctx->category_count = 0;
	lctx->modules = NULL;
	lctx->module_count = 0;
	free(lctx);
	*lctxp = NULL;
}

void
isc_logconfig_destroy(isc_logconfig_t **lcfgp) {
	isc_logconfig_t *lcfg;
	isc_logchannel_t *channel;
	isc_logchannellist_t *item;
	unsigned int i;

	REQUIRE(lcfgp != NULL);

	lcfg = *lcfgp;

	/*
	 * This function cannot be called with a logconfig that is in
	 * use by a log context.
	 */
	REQUIRE(lcfg->lctx != NULL && lcfg->lctx->logconfig != lcfg);

	while ((channel = ISC_LIST_HEAD(lcfg->channels)) != NULL) {
		ISC_LIST_UNLINK(lcfg->channels, channel, link);

		free(channel->name);
		free(channel);
	}

	for (i = 0; i < lcfg->channellist_count; i++)
		while ((item = ISC_LIST_HEAD(lcfg->channellists[i])) != NULL) {
			ISC_LIST_UNLINK(lcfg->channellists[i], item, link);
			free(item);
		}

	if (lcfg->channellist_count > 0)
		free(lcfg->channellists);

	lcfg->dynamic = ISC_FALSE;
	if (lcfg->tag != NULL)
		free(lcfg->tag);
	lcfg->tag = NULL;
	lcfg->highest_level = 0;
	lcfg->duplicate_interval = 0;
	free(lcfg);
	*lcfgp = NULL;
}

void
isc_log_registercategories(isc_log_t *lctx, isc_logcategory_t categories[]) {
	isc_logcategory_t *catp;

	REQUIRE(categories != NULL && categories[0].name != NULL);

	/*
	 * XXXDCL This somewhat sleazy situation of using the last pointer
	 * in one category array to point to the next array exists because
	 * this registration function returns void and I didn't want to have
	 * change everything that used it by making it return an isc_result_t.
	 * It would need to do that if it had to allocate memory to store
	 * pointers to each array passed in.
	 */
	if (lctx->categories == NULL)
		lctx->categories = categories;

	else {
		/*
		 * Adjust the last (NULL) pointer of the already registered
		 * categories to point to the incoming array.
		 */
		for (catp = lctx->categories; catp->name != NULL; )
			if (catp->id == UINT_MAX)
				/*
				 * The name pointer points to the next array.
				 * Ick.
				 */
				DE_CONST(catp->name, catp);
			else
				catp++;

		catp->name = (void *)categories;
		catp->id = UINT_MAX;
	}

	/*
	 * Update the id number of the category with its new global id.
	 */
	for (catp = categories; catp->name != NULL; catp++)
		catp->id = lctx->category_count++;
}

void
isc_log_registermodules(isc_log_t *lctx, isc_logmodule_t modules[]) {
	isc_logmodule_t *modp;

	REQUIRE(modules != NULL && modules[0].name != NULL);

	/*
	 * XXXDCL This somewhat sleazy situation of using the last pointer
	 * in one category array to point to the next array exists because
	 * this registration function returns void and I didn't want to have
	 * change everything that used it by making it return an isc_result_t.
	 * It would need to do that if it had to allocate memory to store
	 * pointers to each array passed in.
	 */
	if (lctx->modules == NULL)
		lctx->modules = modules;

	else {
		/*
		 * Adjust the last (NULL) pointer of the already registered
		 * modules to point to the incoming array.
		 */
		for (modp = lctx->modules; modp->name != NULL; )
			if (modp->id == UINT_MAX)
				/*
				 * The name pointer points to the next array.
				 * Ick.
				 */
				DE_CONST(modp->name, modp);
			else
				modp++;

		modp->name = (void *)modules;
		modp->id = UINT_MAX;
	}

	/*
	 * Update the id number of the module with its new global id.
	 */
	for (modp = modules; modp->name != NULL; modp++)
		modp->id = lctx->module_count++;
}

isc_result_t
isc_log_createchannel(isc_logconfig_t *lcfg, const char *name,
		      unsigned int type, int level,
		      const isc_logdestination_t *destination,
		      unsigned int flags)
{
	isc_logchannel_t *channel;

	REQUIRE(name != NULL);
	REQUIRE(type == ISC_LOG_TOSYSLOG   ||
		type == ISC_LOG_TOFILEDESC || type == ISC_LOG_TONULL);
	REQUIRE(destination != NULL || type == ISC_LOG_TONULL);
	REQUIRE(level >= ISC_LOG_CRITICAL);
	REQUIRE((flags &
		 (unsigned int)~(ISC_LOG_PRINTALL | ISC_LOG_DEBUGONLY)) == 0);

	/* XXXDCL find duplicate names? */

	channel = malloc(sizeof(*channel));
	if (channel == NULL)
		return (ISC_R_NOMEMORY);

	channel->name = strdup(name);
	if (channel->name == NULL) {
		free(channel);
		return (ISC_R_NOMEMORY);
	}

	channel->type = type;
	channel->level = level;
	channel->flags = flags;
	ISC_LINK_INIT(channel, link);

	switch (type) {
	case ISC_LOG_TOSYSLOG:
		FACILITY(channel) = destination->facility;
		break;

	case ISC_LOG_TOFILEDESC:
		FILE_NAME(channel) = NULL;
		FILE_STREAM(channel) = destination->file.stream;
		FILE_MAXSIZE(channel) = 0;
		FILE_VERSIONS(channel) = ISC_LOG_ROLLNEVER;
		break;

	case ISC_LOG_TONULL:
		/* Nothing. */
		break;

	default:
		free(channel->name);
		free(channel);
		return (ISC_R_UNEXPECTED);
	}

	ISC_LIST_PREPEND(lcfg->channels, channel, link);

	/*
	 * If default_stderr was redefined, make the default category
	 * point to the new default_stderr.
	 */
	if (strcmp(name, "default_stderr") == 0)
		default_channel.channel = channel;

	return (ISC_R_SUCCESS);
}

isc_result_t
isc_log_usechannel(isc_logconfig_t *lcfg, const char *name,
		   const isc_logcategory_t *category,
		   const isc_logmodule_t *module)
{
	isc_log_t *lctx;
	isc_logchannel_t *channel;
	isc_result_t result = ISC_R_SUCCESS;
	unsigned int i;

	REQUIRE(name != NULL);

	lctx = lcfg->lctx;

	REQUIRE(category == NULL || category->id < lctx->category_count);
	REQUIRE(module == NULL || module->id < lctx->module_count);

	for (channel = ISC_LIST_HEAD(lcfg->channels); channel != NULL;
	     channel = ISC_LIST_NEXT(channel, link))
		if (strcmp(name, channel->name) == 0)
			break;

	if (channel == NULL)
		return (ISC_R_NOTFOUND);

	if (category != NULL)
		result = assignchannel(lcfg, category->id, module, channel);

	else
		/*
		 * Assign to all categories.  Note that this includes
		 * the default channel.
		 */
		for (i = 0; i < lctx->category_count; i++) {
			result = assignchannel(lcfg, i, module, channel);
			if (result != ISC_R_SUCCESS)
				break;
		}

	return (result);
}

void
isc_log_write(isc_log_t *lctx, isc_logcategory_t *category,
	      isc_logmodule_t *module, int level, const char *format, ...)
{
	va_list args;

	/*
	 * Contract checking is done in isc_log_doit().
	 */

	va_start(args, format);
	isc_log_doit(lctx, category, module, level, ISC_FALSE, format, args);
	va_end(args);
}

void
isc_log_setcontext(isc_log_t *lctx) {
	isc_lctx = lctx;
}

void
isc_log_setdebuglevel(isc_log_t *lctx, unsigned int level) {

	lctx->debug_level = level;
}

/****
 **** Internal functions
 ****/

static isc_result_t
assignchannel(isc_logconfig_t *lcfg, unsigned int category_id,
	      const isc_logmodule_t *module, isc_logchannel_t *channel)
{
	isc_logchannellist_t *new_item;
	isc_log_t *lctx;
	isc_result_t result;

	lctx = lcfg->lctx;

	REQUIRE(category_id < lctx->category_count);
	REQUIRE(module == NULL || module->id < lctx->module_count);
	REQUIRE(channel != NULL);

	/*
	 * Ensure lcfg->channellist_count == lctx->category_count.
	 */
	result = sync_channellist(lcfg);
	if (result != ISC_R_SUCCESS)
		return (result);

	new_item = malloc(sizeof(*new_item));
	if (new_item == NULL)
		return (ISC_R_NOMEMORY);

	new_item->channel = channel;
	new_item->module = module;
	ISC_LIST_INITANDPREPEND(lcfg->channellists[category_id],
			       new_item, link);

	/*
	 * Remember the highest logging level set by any channel in the
	 * logging config, so isc_log_doit() can quickly return if the
	 * message is too high to be logged by any channel.
	 */
	if (channel->type != ISC_LOG_TONULL) {
		if (lcfg->highest_level < channel->level)
			lcfg->highest_level = channel->level;
		if (channel->level == ISC_LOG_DYNAMIC)
			lcfg->dynamic = ISC_TRUE;
	}

	return (ISC_R_SUCCESS);
}

/*
 * This would ideally be part of isc_log_registercategories(), except then
 * that function would have to return isc_result_t instead of void.
 */
static isc_result_t
sync_channellist(isc_logconfig_t *lcfg) {
	unsigned int bytes;
	isc_log_t *lctx;
	void *lists;

	lctx = lcfg->lctx;

	REQUIRE(lctx->category_count != 0);

	if (lctx->category_count == lcfg->channellist_count)
		return (ISC_R_SUCCESS);

	bytes = lctx->category_count * sizeof(ISC_LIST(isc_logchannellist_t));

	lists = malloc(bytes);

	if (lists == NULL)
		return (ISC_R_NOMEMORY);

	memset(lists, 0, bytes);

	if (lcfg->channellist_count != 0) {
		bytes = lcfg->channellist_count *
			sizeof(ISC_LIST(isc_logchannellist_t));
		memmove(lists, lcfg->channellists, bytes);
		free(lcfg->channellists);
	}

	lcfg->channellists = lists;
	lcfg->channellist_count = lctx->category_count;

	return (ISC_R_SUCCESS);
}

isc_boolean_t
isc_log_wouldlog(isc_log_t *lctx, int level) {
	/*
	 * If the level is (mathematically) less than or equal to the
	 * highest_level, or if there is a dynamic channel and the level is
	 * less than or equal to the debug level, the main loop must be
	 * entered to see if the message should really be output.
	 *
	 * NOTE: this is UNLOCKED access to the logconfig.  However,
	 * the worst thing that can happen is that a bad decision is made
	 * about returning without logging, and that's not a big concern,
	 * because that's a risk anyway if the logconfig is being
	 * dynamically changed.
	 */

	if (lctx == NULL || lctx->logconfig == NULL)
		return (ISC_FALSE);

	return (ISC_TF(level <= lctx->logconfig->highest_level ||
		       (lctx->logconfig->dynamic &&
			level <= lctx->debug_level)));
}

static void
isc_log_doit(isc_log_t *lctx, isc_logcategory_t *category,
	     isc_logmodule_t *module, int level, isc_boolean_t write_once,
	     const char *format, va_list args)
{
	int syslog_level;
	char time_string[64];
	char level_string[24];
	const char *iformat;
	isc_boolean_t matched = ISC_FALSE;
	isc_boolean_t printtime, printtag, printcolon;
	isc_boolean_t printcategory, printmodule, printlevel;
	isc_logconfig_t *lcfg;
	isc_logchannel_t *channel;
	isc_logchannellist_t *category_channels;

	REQUIRE(category != NULL);
	REQUIRE(module != NULL);
	REQUIRE(level != ISC_LOG_DYNAMIC);
	REQUIRE(format != NULL);

	/*
	 * Programs can use libraries that use this logging code without
	 * wanting to do any logging, thus the log context is allowed to
	 * be non-existent.
	 */
	if (lctx == NULL)
		return;

	REQUIRE(category->id < lctx->category_count);
	REQUIRE(module->id < lctx->module_count);

	if (! isc_log_wouldlog(lctx, level))
		return;

	iformat = format;

	time_string[0]  = '\0';
	level_string[0] = '\0';

	lctx->buffer[0] = '\0';

	lcfg = lctx->logconfig;

	category_channels = ISC_LIST_HEAD(lcfg->channellists[category->id]);

	/*
	 * XXXDCL add duplicate filtering? (To not write multiple times to
	 * the same source via various channels).
	 */
	do {
		/*
		 * If the channel list end was reached and a match was made,
		 * everything is finished.
		 */
		if (category_channels == NULL && matched)
			break;

		if (category_channels == NULL && ! matched &&
		    category_channels != ISC_LIST_HEAD(lcfg->channellists[0]))
			/*
			 * No category/module pair was explicitly configured.
			 * Try the category named "default".
			 */
			category_channels =
				ISC_LIST_HEAD(lcfg->channellists[0]);

		if (category_channels == NULL && ! matched)
			/*
			 * No matching module was explicitly configured
			 * for the category named "default".  Use the internal
			 * default channel.
			 */
			category_channels = &default_channel;

		if (category_channels->module != NULL &&
		    category_channels->module != module) {
			category_channels = ISC_LIST_NEXT(category_channels,
							  link);
			continue;
		}

		matched = ISC_TRUE;

		channel = category_channels->channel;
		category_channels = ISC_LIST_NEXT(category_channels, link);

		if (((channel->flags & ISC_LOG_DEBUGONLY) != 0) &&
		    lctx->debug_level == 0)
			continue;

		if (channel->level == ISC_LOG_DYNAMIC) {
			if (lctx->debug_level < level)
				continue;
		} else if (channel->level < level)
			continue;

		if ((channel->flags & ISC_LOG_PRINTTIME) != 0 &&
		    time_string[0] == '\0') {
			time_t now;
			now = time(NULL);
			strftime(time_string, sizeof(time_string),
			    "%d-%b-%Y %X", localtime(&now));
		}

		if ((channel->flags & ISC_LOG_PRINTLEVEL) != 0 &&
		    level_string[0] == '\0') {
			if (level < ISC_LOG_CRITICAL)
				snprintf(level_string, sizeof(level_string),
					 "level %d: ", level);
			else if (level > ISC_LOG_DYNAMIC)
				snprintf(level_string, sizeof(level_string),
					 "%s %d: ", log_level_strings[0],
					 level);
			else
				snprintf(level_string, sizeof(level_string),
					 "%s: ", log_level_strings[-level]);
		}

		/*
		 * Only format the message once.
		 */
		if (lctx->buffer[0] == '\0') {
			(void)vsnprintf(lctx->buffer, sizeof(lctx->buffer),
					iformat, args);

			/*
			 * Check for duplicates.
			 */
			if (write_once) {
				isc_logmessage_t *message, *next;
				struct timespec oldest;
				struct timespec interval;
				size_t size;
				interval.tv_sec = lcfg->duplicate_interval;
				interval.tv_nsec = 0;

				/*
				 * 'oldest' is the age of the oldest messages
				 * which fall within the duplicate_interval
				 * range.
				 */
				clock_gettime(CLOCK_MONOTONIC, &oldest);
				timespecsub(&oldest, &interval, &oldest);
				message = ISC_LIST_HEAD(lctx->messages);

				while (message != NULL) {
					if (timespeccmp(&message->time,
					    &oldest, <)) {
						/*
						 * This message is older
						 * than the duplicate_interval,
						 * so it should be dropped from
						 * the history.
						 *
						 * Setting the interval to be
						 * to be longer will obviously
						 * not cause the expired
						 * message to spring back into
						 * existence.
						 */
						next = ISC_LIST_NEXT(message,
								     link);

						ISC_LIST_UNLINK(lctx->messages,
								message, link);

						free(message);

						message = next;
						continue;
					}

					/*
					 * This message is in the duplicate
					 * filtering interval ...
					 */
					if (strcmp(lctx->buffer, message->text)
					    == 0) {
						/*
						 * ... and it is a duplicate.
						 */
						return;
					}

					message = ISC_LIST_NEXT(message, link);
				}

				/*
				 * It wasn't in the duplicate interval,
				 * so add it to the message list.
				 */
				size = sizeof(isc_logmessage_t) +
				       strlen(lctx->buffer) + 1;
				message = malloc(size);
				if (message != NULL) {
					/*
					 * Put the text immediately after
					 * the struct.  The strcpy is safe.
					 */
					message->text = (char *)(message + 1);
					size -= sizeof(isc_logmessage_t);
					strlcpy(message->text, lctx->buffer,
						size);

					clock_gettime(CLOCK_MONOTONIC,
					    &message->time);

					ISC_LINK_INIT(message, link);
					ISC_LIST_APPEND(lctx->messages,
							message, link);
				}
			}
		}

		printtime     = ISC_TF((channel->flags & ISC_LOG_PRINTTIME)
				       != 0);
		printtag      = ISC_TF((channel->flags &
					(ISC_LOG_PRINTTAG|ISC_LOG_PRINTPREFIX))
				       != 0 && lcfg->tag != NULL);
		printcolon    = ISC_TF((channel->flags & ISC_LOG_PRINTTAG)
				       != 0 && lcfg->tag != NULL);
		printcategory = ISC_TF((channel->flags & ISC_LOG_PRINTCATEGORY)
				       != 0);
		printmodule   = ISC_TF((channel->flags & ISC_LOG_PRINTMODULE)
				       != 0);
		printlevel    = ISC_TF((channel->flags & ISC_LOG_PRINTLEVEL)
				       != 0);

		switch (channel->type) {
		case ISC_LOG_TOFILEDESC:
			fprintf(FILE_STREAM(channel),
				"%s%s%s%s%s%s%s%s%s%s\n",
				printtime     ? time_string	: "",
				printtime     ? " "		: "",
				printtag      ? lcfg->tag	: "",
				printcolon    ? ": "		: "",
				printcategory ? category->name	: "",
				printcategory ? ": "		: "",
				printmodule   ? (module != NULL ? module->name
								: "no_module")
								: "",
				printmodule   ? ": "		: "",
				printlevel    ? level_string	: "",
				lctx->buffer);

			fflush(FILE_STREAM(channel));
			break;

		case ISC_LOG_TOSYSLOG:
			if (level > 0)
				syslog_level = LOG_DEBUG;
			else if (level < ISC_LOG_CRITICAL)
				syslog_level = LOG_CRIT;
			else
				syslog_level = syslog_map[-level];

			(void)syslog(FACILITY(channel) | syslog_level,
			       "%s%s%s%s%s%s%s%s%s%s",
			       printtime     ? time_string	: "",
			       printtime     ? " "		: "",
			       printtag      ? lcfg->tag	: "",
			       printcolon    ? ": "		: "",
			       printcategory ? category->name	: "",
			       printcategory ? ": "		: "",
			       printmodule   ? (module != NULL
						 ? module->name
						 : "no_module")
								: "",
			       printmodule   ? ": "		: "",
			       printlevel    ? level_string	: "",
			       lctx->buffer);
			break;

		case ISC_LOG_TONULL:
			break;

		}

	} while (1);
}
