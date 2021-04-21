/*	$OpenBSD: ec_point_conversion.c,v 1.1 2021/04/21 20:15:08 tb Exp $ */
/*
 * Copyright (c) 2021 Theo Buehler <tb@openbsd.org>
 * Copyright (c) 2021 Joel Sing <jsing@openbsd.org>
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

#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include <openssl/ec.h>
#include <openssl/objects.h>

int bn_rand_interval(BIGNUM *, const BIGNUM *, const BIGNUM *);

int forms[] = {
	POINT_CONVERSION_COMPRESSED,
	POINT_CONVERSION_UNCOMPRESSED,
	POINT_CONVERSION_HYBRID,
};

static const size_t N_FORMS = sizeof(forms) / sizeof(forms[0]);
#define N_RANDOM_POINTS 10

static const char *
form2str(int form)
{
	switch (form) {
	case POINT_CONVERSION_COMPRESSED:
		return "compressed form";
	case POINT_CONVERSION_UNCOMPRESSED:
		return "uncompressed form";
	case POINT_CONVERSION_HYBRID:
		return "hybrid form";
	default:
		return "unknown form";
	}
}

static void
hexdump(const unsigned char *buf, size_t len)
{
	size_t i;

	for (i = 1; i <= len; i++)
		fprintf(stderr, " 0x%02hhx,%s", buf[i - 1], i % 8 ? "" : "\n");

	if (len % 8)
		fprintf(stderr, "\n");
}

static int
roundtrip(EC_GROUP *group, EC_POINT *point, int form, BIGNUM *x, BIGNUM *y)
{
	BIGNUM *x_out = NULL, *y_out = NULL;
	size_t len;
	uint8_t *buf = NULL;
	int failed = 1;

	if ((len = EC_POINT_point2oct(group, point, form, NULL, 0, NULL)) == 0)
		errx(1, "point2oct");
	if ((buf = malloc(len)) == NULL)
		errx(1, "malloc");
	if (EC_POINT_point2oct(group, point, form, buf, len, NULL) != len)
		errx(1, "point2oct");

	if (!EC_POINT_oct2point(group, point, buf, len, NULL))
		errx(1, "%s oct2point", form2str(form));

	if ((x_out = BN_new()) == NULL)
		errx(1, "new x_out");
	if ((y_out = BN_new()) == NULL)
		errx(1, "new y_out");

	if (!EC_POINT_get_affine_coordinates(group, point, x_out, y_out, NULL))
		errx(1, "get affine");

	if (BN_cmp(x, x_out) != 0) {
		warnx("%s: x", form2str(form));
		goto err;
	}
	if (BN_cmp(y, y_out) != 0) {
		warnx("%s: y", form2str(form));
		goto err;
	}

	failed = 0;

 err:
	if (failed)
		hexdump(buf, len);

	free(buf);
	BN_free(x_out);
	BN_free(y_out);

	return failed;
}

static int
hybrid_corner_case(void)
{
	BIGNUM *x = NULL, *y = NULL;
	EC_GROUP *group;
	EC_POINT *point;
	size_t i;
	int failed = 0;

	if (!BN_hex2bn(&x, "0"))
		errx(1, "BN_hex2bn x");
	if (!BN_hex2bn(&y, "01"))
		errx(1, "BN_hex2bn y");

	if ((group = EC_GROUP_new_by_curve_name(NID_sect571k1)) == NULL)
		errx(1, "group");
	if ((point = EC_POINT_new(group)) == NULL)
		errx(1, "point");

	if (!EC_POINT_set_affine_coordinates(group, point, x, y, NULL))
		errx(1, "set affine");

	for (i = 0; i < N_FORMS; i++)
		failed |= roundtrip(group, point, forms[i], x, y);

	fprintf(stderr, "%s: %s\n", __func__, failed ? "FAILED" : "SUCCESS");

	EC_GROUP_free(group);
	EC_POINT_free(point);
	BN_free(x);
	BN_free(y);

	return failed;
}

/* XXX This only tests multiples of the generator for now... */
static int
test_random_points_on_curve(EC_builtin_curve *curve)
{
	EC_GROUP *group;
	BIGNUM *order = NULL;
	BIGNUM *random;
	BIGNUM *x, *y;
	size_t i, j;
	int failed = 0;

	fprintf(stderr, "%s\n", OBJ_nid2sn(curve->nid));
	if ((group = EC_GROUP_new_by_curve_name(curve->nid)) == NULL)
		errx(1, "EC_GROUP_new_by_curve_name");

	if ((order = BN_new()) == NULL)
		errx(1, "BN_new order");
	if ((random = BN_new()) == NULL)
		errx(1, "BN_new random");
	if ((x = BN_new()) == NULL)
		errx(1, "BN_new x");
	if ((y = BN_new()) == NULL)
		errx(1, "BN_new y");

	if (!EC_GROUP_get_order(group, order, NULL))
		errx(1, "EC_group_get_order");

	for (i = 0; i < N_RANDOM_POINTS; i++) {
		EC_POINT *random_point;

		if (!bn_rand_interval(random, BN_value_one(), order))
			errx(1, "bn_rand_interval");

		if ((random_point = EC_POINT_new(group)) == NULL)
			errx(1, "EC_POINT_new");

		if (!EC_POINT_mul(group, random_point, random, NULL, NULL, NULL))
			errx(1, "EC_POINT_mul");

		if (EC_POINT_is_at_infinity(group, random_point)) {
			EC_POINT_free(random_point);

			warnx("info: got infinity");
			fprintf(stderr, "random = ");
			BN_print_fp(stderr, random);
			fprintf(stderr, "\n");

			continue;
		}

		if (!EC_POINT_get_affine_coordinates(group, random_point,
		    x, y, NULL))
			errx(1, "EC_POINT_get_affine_coordinates");

		for (j = 0; j < N_FORMS; j++)
			failed |= roundtrip(group, random_point, forms[j], x, y);

		EC_POINT_free(random_point);
	}

	BN_free(order);
	BN_free(random);
	BN_free(x);
	BN_free(y);
	EC_GROUP_free(group);

	return failed;
}

static int
test_random_points(void)
{
	EC_builtin_curve *all_curves = NULL;
	size_t ncurves = 0;
	size_t curve_id;
	int failed = 0;

	ncurves = EC_get_builtin_curves(NULL, 0);
	if ((all_curves = calloc(ncurves, sizeof(EC_builtin_curve))) == NULL)
		err(1, "calloc builtin curves");
	EC_get_builtin_curves(all_curves, ncurves);

	for (curve_id = 0; curve_id < ncurves; curve_id++)
		test_random_points_on_curve(&all_curves[curve_id]);

	fprintf(stderr, "%s: %s\n", __func__, failed ? "FAILED" : "SUCCESS");

	free(all_curves);
	return failed;
}

int
main(int argc, char **argv)
{
	int failed = 0;

	failed |= test_random_points();
	failed |= hybrid_corner_case();

	fprintf(stderr, "%s\n", failed ? "FAILED" : "SUCCESS");

	return failed;
}
