/*	$OpenBSD: trace.c,v 1.3 2015/01/16 16:18:07 deraadt Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include "syscall.h"
#include "resolve.h"
#include "util.h"

/*
 * Library call tracing routines.
 */

static	int _dl_traceplt;

struct tracespec {
	int	inverse;	/* blacklist instead of whitelist */
	char	*spec;		/* comma separated spec entries */
};

static struct tracespec _dl_tracelib, _dl_tracefunc;

static const char *_dl_trace_parse_spec(const char *, struct tracespec *);
static int _dl_trace_match(const char *, struct tracespec *, int);

void
_dl_trace_setup(char **envp)
{
	const char *var;
	int inherit;

	var = _dl_getenv("LD_TRACE_PLT", envp);
	if (var == NULL)
		return;

	if (!_dl_trust) {
		_dl_unsetenv("LD_TRACE_PLT", envp);
		return;
	}

	_dl_traceplt = 1;

	/*
	 * We expect LD_TRACE_PLT to be empty unless trace inheritance has
	 * been setup by ltrace(1).  We can then clear the environment
	 * variable to avoid useless work in our children, should we fork
	 * any.
	 */
	inherit = *var != '\0';
	if (!inherit)
		_dl_unsetenv("LD_TRACE_PLT", envp);

	/*
	 * Check for a fine-grained trace specification, and extract the
	 * library and function lists, if any.
	 */

	var = _dl_getenv("LD_TRACE_PLTSPEC", envp);
	if (var != NULL) {
		var = _dl_trace_parse_spec(var, &_dl_tracelib);
		(void)_dl_trace_parse_spec(var, &_dl_tracefunc);
		if (!inherit)
			_dl_unsetenv("LD_TRACE_PLTSPEC", envp);
	}
}

void
_dl_trace_object_setup(elf_object_t *object)
{
	const char *basename, *slash;

	object->traced = 0;

	if (_dl_traceplt) {
		basename = object->load_name;
		while (*basename == '/') {
			basename++;
			slash = _dl_strchr(basename, '/');
			if (slash == NULL)
				break;
			basename = slash;
		}
		if (_dl_trace_match(basename, &_dl_tracelib, 1))
			object->traced = 1;
	}
}

int
_dl_trace_plt(const elf_object_t *object, const char *symname)
{
	if (!_dl_trace_match(symname, &_dl_tracefunc, 0))
		return 0;

	_dl_utrace(".plt object",
	    object->load_name, _dl_strlen(object->load_name));
	_dl_utrace(".plt symbol",
	    symname, _dl_strlen(symname));

	return 1;	/* keep tracing */
}

/*
 * Extract a trace specification field, and setup the tracespec struct
 * accordingly.
 */
const char *
_dl_trace_parse_spec(const char *var, struct tracespec *spec)
{
	const char *start, *end;

	if (*var == '!') {
		spec->inverse = 1;
		var++;
	}

	start = var;
	end = _dl_strchr(start, ':');
	if (end == NULL)
		end = start + _dl_strlen(start);

	if (end != start) {
		spec->spec = _dl_malloc(1 + end - start);
		if (spec->spec == NULL)
			_dl_exit(8);

		_dl_bcopy(start, spec->spec, end - start);
		spec->spec[end - start] = '\0';
	}

	if (*end == ':')
		end++;

	return end;
}

/*
 * Check if a given name matches a trace specification list.
 */
static int
_dl_trace_match(const char *name, struct tracespec *spec, int allow_so)
{
	const char *list, *end, *next;
	size_t span;
	int match;

	/* no spec means trace everything */
	if (spec->spec == NULL)
		return 1;

	match = 0;
	list = spec->spec;
	end = list + _dl_strlen(list);

	while (*list != '\0') {
		next = _dl_strchr(list, ',');
		if (next == NULL)
			next = end;

		span = next - list;
		if (span != 0 && *(next - 1) == '*')
			span--;

		if (span != 0 && _dl_strncmp(name, list, span) == 0) {
			/*
			 * If the object name matches the specification
			 * fragment so far, it's a match if:
			 *   + the specification ends in a star (wildcard
			 *     match)
			 *   + there are no remaining chars in both the
			 *     object name and the specification (exact
			 *     match)
			 *   + the specification ends (no star) and the
			 *     object name continues with ".so" (radix
			 *     match) and `allow_so' is nonzero.
			 */
			if (list[span] == '*' ||
			    name[span] == '\0' ||
			    (allow_so &&
			    _dl_strncmp(name + span, ".so", 3) == 0)) {
				match = 1;
				break;
			}
		}

		while (*next == ',')
			next++;
		list = next;
	}

	return spec->inverse ? !match : match;
}
