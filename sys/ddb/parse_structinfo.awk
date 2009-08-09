#	$OpenBSD: parse_structinfo.awk,v 1.1 2009/08/09 23:04:49 miod Exp $
#
# Copyright (c) 2009 Miodrag Vallat.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

# This ugly script parses the output of objdump -g in order to extract
# structure layout information, to be used by ddb.
#
# The output of this script is the following static data:
# - for each struct:
#   - its name
#   - its size (individual element size if an array)
#   - the number of elements in the array (1 if not)
#   - its first and last field indexes
# - for each field:
#   - its name
#   - its offset and size
#   - the index of the struct it is member of
# This allows fast struct -> field information retrieval.
#
# To retrieve information from a field size or offset, we also output
# the following reverse arrays:
# - for each offset, in ascending order, a variable length list of field
#   indexes.
# - for each size, in ascending order, a variable length list of field
#   indexes.
#
# The compromise here is that I want to minimize linear searches. Memory
# use is considered secondary, hence the back `pointer' to the struct in the
# fields array.
#
# No attempt is made to share name pointers when multiple fields share
# the same name. Rewriting this script in perl, or any other language
# with hash data structures, would be a good time to add this.

BEGIN {
	depth = 0;
	ignore = 0;
	scnt = 0;	# current struct count
	sidx = -1;	# current struct index
	# field index #0 is used as a sentinel.
	fcnt = 1;	# current field count
	fidx = 0;	# current field index
	ocnt = 0;	# current offset count
	zcnt = 0;	# current size count
	
}
/^struct / {
	depth = 1;
	sidx = scnt;
	sname[sidx] = $2;
	ssize[sidx] = $6;
	sfieldmin[sidx] = fcnt;
	scnt++;
	#printf("struct %d %s (size %d)\n", sidx, $2, $6);
	next;
}
/^};/ {
	if (depth != 0) {
		depth = 0;
		if (fcnt == sfieldmin[sidx])	# empty struct, ignore it
			scnt--;
		else
			sfieldmax[sidx] = fidx;
	} else
		ignore--;
	next;
}
/{.*}/ {
	# single line enum
	next;
}
/{/ {
	# subcomponent
	if (depth != 0) {
		depth++;
	} else {
		ignore++;
	}
	next;
}
/}/ {
	if (ignore != 0) {
		ignore--;
		next;
	} else {
		depth--;
	}
	if (depth != 1)
		next;
	# FALLTHROUGH
}
/bitsize/ {
	if (ignore != 0)
		next;
	if (depth != 1)
		next;

	# Bitfields are a PITA... From a ddb point of view, we can't really
	# access storage units smaller than a byte.
	# So we'll report all bitfields as having size 0, and the
	# rounded down byte position where they start.
	cursize = int($(NF - 3));
	curoffs = int($(NF - 1) / 8);
	if ((cursize % 8) != 0)
		cursize = 0;
	else
		cursize /= 8;

	# try and gather the field name.
	field = $(NF - 6);
	if (field == "};") {
		# anonymous union. discard it.
		next;
	}
	if (field == "*/);") {
		field = $(NF - 9);	# function pointer
		# remove enclosing braces
		field = substr(field, 2, length(field) - 2);
	}
	colon = index(field, ":");
	if (colon != 0)
		field = substr(field, 1, colon - 1);
	else if (substr(field, length(field), 1) == ";")
		field = substr(field, 1, length(field) - 1);

	while (index(field, "*") == 1)
		field = substr(field, 2);

	# This could be an array. If it is, we need to trim the field
	# name and update its size to a single member size.
	obracket = index(field, "[");
	cbracket = index(field, "]");
	if (obracket != 0) {
		obracket++;
		nitems = substr(field, obracket, cbracket - obracket);
		field = substr(field, 1, obracket - 2);
		cursize /= nitems;
	} else
		nitems = 1;

	fidx = fcnt;
	fname[fidx] = field;
	foffs[fidx] = curoffs;
	fsize[fidx] = cursize;
	fitems[fidx] = nitems;
	fstruct[fidx] = sidx;
	fcnt++;
	#printf("  %s at %d len %d\n", field, curoffs, cursize);

	# Remember size and offset if not found yet

	for (i = 0; i < ocnt; i++)
		if (offs[i] == curoffs)
			break;
	if (i == ocnt) {
		# keep array sorted
		for (i = 0; i < ocnt; i++)
			if (offs[i] > curoffs)
				break;
		if (i < ocnt) {
			for (j = ocnt + 1; j > i; j--)
				offs[j] = offs[j - 1];
		}
		offs[i] = curoffs;
		ocnt++;
	}

	for (i = 0; i < zcnt; i++)
		if (sizes[i] == cursize)
			break;
	if (i == zcnt) {
		# keep array sorted
		for (i = 0; i < zcnt; i++)
			if (sizes[i] > cursize)
				break;
		if (i < zcnt) {
			for (j = zcnt + 1; j > i; j--)
				sizes[j] = sizes[j - 1];
		}
		sizes[i] = cursize;
		zcnt++;
	}
}
END {
	printf("/*\n");
	printf(" * THIS IS A GENERATED FILE.  DO NOT EDIT!\n");
	printf(" */\n\n");

	printf("#include <sys/param.h>\n");
	printf("#include <sys/types.h>\n");
	printf("\n");

	# structure definitions

	printf("struct ddb_struct_info {\n");
	printf("\tconst char *name;\n");
	printf("\tsize_t size;\n");
	printf("\tuint fmin, fmax;\n");
	printf("};\n");

	printf("struct ddb_field_info {\n");
	printf("\tconst char *name;\n");
	printf("\tuint sidx;\n");
	printf("\tsize_t offs;\n");
	printf("\tsize_t size;\n");
	printf("\tuint nitems;\n");
	printf("};\n");

	printf("struct ddb_field_offsets {\n");
	printf("\tsize_t offs;\n");
	printf("\tconst uint *list;\n");
	printf("};\n");

	printf("struct ddb_field_sizes {\n");
	printf("\tsize_t size;\n");
	printf("\tconst uint *list;\n");
	printf("};\n");

	# forward arrays

	printf("#define NSTRUCT %d\n", scnt);
	printf("static const struct ddb_struct_info ddb_struct_info[NSTRUCT] = {\n");
	for (i = 0; i < scnt; i++) {
		printf("\t{ \"%s\", %d, %d, %d },\n",
		    sname[i], ssize[i], sfieldmin[i], sfieldmax[i]);
	}
	printf("};\n\n");

	printf("#define NFIELD %d\n", fcnt);
	printf("static const struct ddb_field_info ddb_field_info[NFIELD] = {\n");
	printf("\t{ NULL, 0, 0, 0 },\n");
	for (i = 1; i < fcnt; i++) {
		printf("\t{ \"%s\", %d, %d, %d, %d },\n",
		    fname[i], fstruct[i], foffs[i], fsize[i], fitems[i]);
	}
	printf("};\n\n");

	# reverse arrays

	printf("static const uint ddb_fields_by_offset[] = {\n");
	w = 0;
	for (i = 0; i < ocnt; i++) {
		cmp = offs[i];
		ohead[i] = w;
		for (f = 1; f < fcnt; f++)
			if (foffs[f] == cmp) {
				if ((w % 10) == 0)
					printf("\t");
				printf("%d, ", f);
				w++;
				if ((w % 10) == 0)
					printf("\n");
			}
		if ((w % 10) == 0)
			printf("\t");
		printf("0, ");
		w++;
		if ((w % 10) == 0)
			printf("\n");
	}
	if ((w % 10) != 0)
		printf("\n");
	printf("};\n\n");

	printf("#define NOFFS %d\n", ocnt);
	printf("static const struct ddb_field_offsets ddb_field_offsets[NOFFS] = {\n");
	for (i = 0; i < ocnt; i++) {
		printf("\t{ %d, ddb_fields_by_offset + %d },\n",
		    offs[i], ohead[i]);
	}
	printf("};\n\n");

	printf("static const uint ddb_fields_by_size[] = {\n");
	w = 0;
	for (i = 0; i < zcnt; i++) {
		cmp = sizes[i];
		zhead[i] = w;
		for (f = 1; f < fcnt; f++)
			if (fsize[f] == cmp) {
				if ((w % 10) == 0)
					printf("\t");
				printf("%d, ", f);
				w++;
				if ((w % 10) == 0)
					printf("\n");
			}
		if ((w % 10) == 0)
			printf("\t");
		printf("0, ");
		w++;
		if ((w % 10) == 0)
			printf("\n");
	}
	if ((w % 10) != 0)
		printf("\n");
	printf("};\n\n");

	printf("#define NSIZES %d\n", zcnt);
	printf("static const struct ddb_field_sizes ddb_field_sizes[NSIZES] = {\n");
	for (i = 0; i < zcnt; i++) {
		printf("\t{ %d, ddb_fields_by_size + %d },\n",
		    sizes[i], zhead[i]);
	}
	printf("};\n");
}
