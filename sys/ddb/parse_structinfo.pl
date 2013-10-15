#!/usr/bin/perl
#	$OpenBSD: parse_structinfo.pl,v 1.1 2013/10/15 19:23:24 guenther Exp $
#
# Copyright (c) 2009 Miodrag Vallat.
# Copyright (c) 2013 Philip Guenther.
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

use strict;
use warnings;
use integer;

use constant MAX_COLUMN => 72;

my $depth = 0;
my $ignore = 0;

my $cur_struct;

my $max_offs = 0;
my $max_fsize = 0;
my $max_ssize = 0;

# count of how many times each literal string appears
my %strings;
my @strings_by_len;
sub add_string
{
    my $string = shift;
    if ($strings{$string}++ == 0) {
	push @{ $strings_by_len[ length $string ] }, $string;
    }
}

my @structs;

my %offs_to_fields;
my %size_to_fields;
my @fields = ( {
	name	=> "",
	offs	=> 0,
	size	=> 0,
	items	=> 0,
	struct	=> 0,
    } );
sub new_field
{
	my($name, $offs, $size, $items) = @_;

	add_string($name);
	push @fields, {
		name	=> $name,
		offs	=> $offs,
		size	=> $size,
		items	=> $items // 1,
		struct	=> scalar(@structs),
	    };
	$max_offs = $offs	if $offs > $max_offs;
	$max_fsize = $size	if $size > $max_fsize;
	push @{ $offs_to_fields{$offs} }, $#fields;
	push @{ $size_to_fields{$size} }, $#fields;
}

while (<>) {
    chomp;	# strip record separator
    if (m!^struct (\w+) \{ /\* size (\d+) !) {
	$depth = 1;
	$cur_struct = {
		name	 => $1,
		size	 => $2,
		fieldmin => scalar(@fields)
	    };
	next
    }

    if (/^};/) {
	if ($depth == 0) {
	    $ignore--;
	    next
	}
	$depth = 0;
	if (scalar(@fields) == $cur_struct->{fieldmin}) {
	    # empty struct, ignore it
	    undef $cur_struct;
	    next
	}
	$cur_struct->{fieldmax} = $#fields;
	add_string( $cur_struct->{name} );
	$max_ssize = $cur_struct->{size} if $cur_struct->{size} > $max_ssize;
	push @structs, $cur_struct;
	next
    }

    next if /\{.*\}/;		# single line enum

    if (/\{/) {
	# subcomponent
	if ($depth) {
	    $depth++;
	} else {
	    $ignore++;
	}
	next
    }

    if (/\}/) {
	if ($ignore) {
	    $ignore--;
	    next
	}
	$depth--;
	next if $depth != 1;
	# FALL THROUGH
    }

    if (/bitsize (\d+), bitpos (\d+)/) {
	next if $ignore;
	next if $depth != 1;

	# Bitfields are a PITA... From a ddb point of view, we can't really
	# access storage units smaller than a byte.
	# So we'll report all bitfields as having size 0, and the
	# rounded down byte position where they start.
	my $cursize = ($1 % 8) ? 0 : ($1 / 8);
	my $curoffs = $2 / 8;

	# Try and gather the field name.
	# The most common case: not a function pointer or array
	if (m!\s\**(\w+);\s/\* bitsize!) {
	    new_field($1, $curoffs, $cursize);
	    next
	}

	# How about a function pointer?
	if (m!\s\**\(\*+(\w+)\) \(/\* unknown \*/\);\s/\* bitsize!) {
	    new_field($1, $curoffs, $cursize);
	    next
	}

	# Maybe it's an array
	if (m!\s\**([][:\w]+);\s/\* bitsize!) {
	    my $name = $1;
	    my $items = 1;
	    while ($name =~ s/\[(\d+)\]:\w+//) {
		$items *= $1;
	    }
	    new_field($name, $curoffs, $cursize / $items, $items);
	    next
	}

	# skip any anonymous unions {
	next if m!\}; /\*!;

	# Should be nothing left
	print STDERR "unknown member type: $_\n";
	next
    }
}

print <<EOM;
/*
 * THIS IS A GENERATED FILE.  DO NOT EDIT!
 */

#include <sys/param.h>
#include <sys/types.h>

struct ddb_struct_info {
	u_short name;
	u_short size;
	u_short fmin, fmax;
};
struct ddb_field_info {
	u_short name;
	u_short sidx;
	u_short offs;
	u_short size;
	u_short nitems;
};
struct ddb_field_offsets {
	u_short offs;
	u_short list;
};
struct ddb_field_sizes {
	u_short size;
	u_short list;
};
EOM

my $prefix = qq(static const char ddb_structfield_strings[] =\n\t"\\0);
my %string_to_offset = ( "" => 0 );
my $soff = 1;
for (my $len = $#strings_by_len; $len > 0; $len--) {
    foreach my $string (@{ $strings_by_len[$len] }) {
	next if exists $string_to_offset{$string};
	my $off = $string_to_offset{$string} = $soff;
	$soff += $len + 1;	# for the NUL
	print $prefix, $string;
	$prefix = qq(\\0"\n\t");

	# check for suffixes that are also strings
	for (my $o = 1; $o < $len; $o++) {
	    my $sstr = substr($string, $o);
	    next unless exists $strings{$sstr};
	    next if exists $string_to_offset{$sstr};
	    $string_to_offset{$sstr} = $off + $o;
	    #print STDERR "found $sstr inside $string\n";
	}
    }
}
print qq(";\n);

sub resolve_string
{
    my $string = shift;
    if (! exists $string_to_offset{$string}) {
	die "no mapping for $string";
    }
    return $string_to_offset{$string};
}

# Check for overflow and, if so, print some stats
if ($soff > 65535 || $max_offs > 65535 || $max_fsize > 65535 ||
    $max_ssize > 65535 || @structs > 65535 || @fields > 65535) {
    print STDERR <<EOM;
ERROR: value of range of u_short  Time to change types?

max string offset: $soff
max field offset: $max_offs
max field size: $max_fsize
max struct size: $max_ssize
number of structs: ${\scalar(@structs)}
number of fields: ${\scalar(@fields)}
EOM
    exit 1
}


print "#define NSTRUCT ", scalar(@structs), "\n";
print "static const struct ddb_struct_info ddb_struct_info[NSTRUCT] = {\n";

foreach my $s (@structs) {
    my $name = resolve_string($s->{name});
    print "\t{ ",
	join(", ", $name, @{$s}{qw( size fieldmin fieldmax )}),
        " },\n";
}
printf "};\n\n";

print "#define NFIELD ", scalar(@fields), "\n";
print "static const struct ddb_field_info ddb_field_info[NFIELD] = {\n";
foreach my $f (@fields) {
    my $name = resolve_string($f->{name});
    print "\t{ ",
	join(", ", $name, @{$f}{qw( struct offs size items )}),
        " },\n";
}
printf "};\n\n";


# Given a mapping from values to fields that have that value, generate
# two C arrays, one containing lists of fields which each value, in order,
# the other indexing into that one for each value.  I.e., to get the
# fields that have a given value, find the value in the second array and
# then iterate from where that points into the first array until you hit
# an entry with field==0.
sub print_reverse_mapping
{
    my($prefix, $map, $max) = @_;
    print "static const u_short ddb_fields_by_${prefix}[] = {";
    my @heads;
    my $w = 0;
    foreach my $val (sort { $a <=> $b } keys %$map) {
	push @heads, [$val, $w];
	foreach my $field (@{ $map->{$val} }, 0) {
	    print( ($w++ % 10) == 0 ? "\n\t" : " ", $field, ",");
	}
    }
    print "\n};\n\n";
    print "#define $max ", scalar(@heads), "\n";
    print "static const struct ddb_field_${prefix}s",
		" ddb_field_${prefix}s[$max] = {\n";
    foreach my $h (@heads) {
	#print "\t{ $h->[0], ddb_fields_by_${prefix} + $h->[1] },\n";
	print "\t{ $h->[0], $h->[1] },\n";
    }
    print "};\n";
}

# reverse arrays
print_reverse_mapping("offset", \%offs_to_fields, "NOFFS");
print "\n";

# The size->field mapping isn't used by ddb currently, so don't output it
# print_reverse_mapping("size", \%size_to_fields, "NSIZES");

