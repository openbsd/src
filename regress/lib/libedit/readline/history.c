/*
 * Copyright (c) 2016 Bastian Maerkisch <bmaerkisch@web.de>
 *
 * Permission to use, copy, modify, and distribute these tests for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THESE TESTS ARE PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <err.h>
#include <string.h>

#ifdef READLINE
#include <stdio.h>
#include <readline/history.h>
#else
#include <editline/readline.h>
#endif


/* Fails if previous and next are interchanged. */
int
test_movement_direction(void)
{
	HIST_ENTRY	*he;
	int		 ok = 1;

	using_history();
	add_history("111");
	add_history("222");

	/* Move to the oldest entry. */
	while (previous_history() != NULL);
	he = current_history();
	if (he == NULL || strcmp(he->line, "111") != 0)
		ok = 0;

	/*
	 * Move to the most recent end of the history.
	 * This moves past the newest entry.
	 */
	while (next_history() != NULL);
	he = current_history();
	if (he != NULL)
		ok = 0;

	clear_history();
	return ok;
}


/* Fails if the position is counted from the recent end. */
int
test_where(void)
{
	int		 ok = 1;

	using_history();

	/*
	 * Adding four elements since set_pos(0) doesn't work
	 * for some versions of libedit.
	 */
	add_history("111");
	add_history("222");
	add_history("333");
	add_history("444");

	/* Set the pointer to the element "222". */
	history_set_pos(1);
	if (where_history() != 1)
		ok = 0;

	clear_history();
	return ok;
}


/* Fails if set_pos returns 0 for success and -1 for failure. */
int
test_set_pos_return_values(void)
{
	int		 ok = 1;
	int		 ret;

	using_history();
	add_history("111");
	add_history("222");

	/* This should fail. */
	ret = history_set_pos(-1);
	if (ret != 0)
		ok = 0;

	/*
	 * This should succeed.
	 * Note that we do not use the index 0 here, since that
	 * actually fails for some versions of libedit.
	 */
	ret = history_set_pos(1);
	if (ret != 1)
		ok = 0;

	clear_history();
	return ok;
}


/* Fails if the index is one-based. */
int
test_set_pos_index(void)
{
	HIST_ENTRY	*he;
	int		 ok = 1;

	using_history();
	add_history("111");
	add_history("222");

	/* Do not test return value here since that might be broken, too. */
	history_set_pos(0);
	he = current_history();
	if (he == NULL || strcmp(he->line, "111") != 0)
		ok = 0;

	history_set_pos(1);
	he = current_history();
	if (he == NULL || strcmp(he->line, "222") != 0)
		ok = 0;

	clear_history();
	return ok;
}


/* Fails if remove does not renumber. */
int
test_remove(void)
{
	HIST_ENTRY	*he;
	int		 ok = 1;

	using_history();
	add_history("111");
	add_history("222");
	add_history("333");
	add_history("444");

	/* Remove the second item "222"; the index is zero-based. */
	remove_history(1);

	/*
	 * Try to get the new second element using history_get.
	 * The argument of get is based on history_base.
	 */
	he = history_get(history_base + 1);
	if (he == NULL || strcmp(he->line, "333") != 0)
		ok = 0;

	/*
	 * Try to get the second element using set_pos/current.
	 * The index is zero-based.
	 */
	history_set_pos(1);
	he = current_history();
	if (he == NULL || strcmp(he->line, "333") != 0)
		ok = 0;

	/* Remove the new second item "333". */
	remove_history(1);
	he = history_get(history_base + 1);
	if (he == NULL || strcmp(he->line, "444") != 0)
		ok = 0;

	clear_history();
	return ok;
}


/* Fails if stifle doesn't discard existing entries. */
int
test_stifle_size(void)
{
	int		 ok = 1;

	using_history();
	unstifle_history();
	add_history("111");
	add_history("222");
	add_history("333");

	/* Reduce the size of the history. */
	stifle_history(2);
	if (history_length != 2)
		ok = 0;

	unstifle_history();
	clear_history();
	return ok;
}


/* Fails if add doesn't increase history_base if the history is full. */
int
test_stifle_base(void)
{
	int		 ok = 1;

	using_history();
	stifle_history(2);

	/* Add one more than the maximum size. */
	add_history("111");
	add_history("222");
	add_history("333");

	/* The history_base should have changed. */
	if (history_base != 2)
		ok = 0;

	unstifle_history();
	clear_history();
	return ok;
}


int
main(void)
{
	int		 fail = 0;

	if (!test_movement_direction()) {
		warnx("previous or next move to the wrong entry.");
		fail++;
	}
	if (!test_where()) {
		warnx("where returns the wrong history number.");
		fail++;
	}
	if (!test_set_pos_return_values()) {
		warnx("set_pos returns the wrong history number.");
		fail++;
	}
	if (!test_set_pos_index()) {
		warnx("set_pos moves to the wrong entry.");
		fail++;
	}
	if (!test_remove()) {
		warnx("remove corrupts history numbers.");
		fail++;
	}
	if (!test_stifle_size()) {
		warnx("stifle sets the wrong history size.");
		fail++;
	}
	if (!test_stifle_base()) {
		warnx("add to a stifled history sets the wrong history_base.");
		fail++;
	}
	if (fail)
		errx(1, "%d test(s) failed.", fail);
	return 0;
}
