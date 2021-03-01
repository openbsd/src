use Data::Dumper;
$Data::Dumper::Sortkeys=1;
################################################################################
#
#  mkppport_fnc.pl -- generate ppport.fnc
#
# This program should be run when regenerating the data for ppport.h
# (devel/regenerate).  It should be run after parts/embed.fnc is updated, and
# after mkapidoc.pl has been run.
#
# Its purpose is to generate ppport.fnc, a file which has the same syntax as
# embed.fnc and apidoc.fnc, but contains entries that should only be tested
# when ppport.h is enabled during the test.
#
# Thus it includes items that are Devel::PPPort only, and items that it
# figures out aren't tested by the other two functions.
#
# These otherwise-untested items are those:
#   1) which D:P provides and are not found in embed.fnc nor apidoc.fnc, or
#      aren't listed as public API in those files
#   2) and for which tests can be automatically generated that they at least
#      compile.
#
# The reason that an item isn't in those two files is that it is an
# undocumented macro.  (If it's not a macro, it has to be in embed.fnc, and if
# it's documented, mkapidoc.sh would find it and place it in apidoc.fnc.)
#
# And, the reason we can't generate tests for undocumented macros is we don't
# readily know the types of the parameters, which we need to get a C program
# to compile.  We could easily discover the number of parameters, but gleaning
# their types is harder.
#
# Instead of expending effort to cope with undocumented items, document them
# instead, improving the product doubly.
#
# However, if the macro has no parameters, there are no types to need to know.
# And, it turns out, that it may be that many of these macros (which typically
# just define constants) really don't need to be documented.  They may be
# something that is considered to be provided, but should actually have been
# internal constants, not exposed to the outside world.  And they're a lot of
# them.  So this function was written to handle them.
#
# Algorithms could be devised to read the =xsubs sections and associate code
# found therein with the item, and to include the code as the test for the
# item, but again, it would be better to just document them.
#
# Later it was discovered that ppport provides support for non-public items.
# We can list those here too, so that tests can be generated.  (An alternative
# would be to invent a new flag that means non-public, but test and use that
# in apidoc.fnc.)
################################################################################
#
#  This program is free software; you can redistribute it and/or
#  modify it under the same terms as Perl itself.
#
################################################################################

use strict;
    use Data::Dumper;
    $Data::Dumper::Sortkeys=1;
use warnings;

my $main_dir = $0;

# Up one level
$main_dir =~ s;[^/]*$;;;
$main_dir =~ s;/$;;;

# Up a second level
$main_dir =~ s;[^/]*$;;;
$main_dir =~ s;/$;;;

$main_dir = '.' unless $main_dir;
require "$main_dir/parts/ppptools.pl";


my @provided = map { /^(\w+)/ ? $1 : () } `$^X ppport.h --list-provided`;

# First, we look for non-API macros that are documented and furnished by us in
# spite of not being public
my @non_public_provided;
my $api_fnc = "$main_dir/parts/apidoc.fnc";
open F, "<",  $api_fnc or die "Can't open $api_fnc: $!";
while (<F>) {
    my $line = $_;
    next if $line =~ / ^ [^|]* A /x;    # Skip API
    chomp $line;
    push @non_public_provided, $line
        # Look for the name in the third '|' separated field
        if grep { $line =~ / ^ [^|]* \| [^|]* \| \s* $_ \s* (?: $ |\| ) /x }
                                                                    @provided;
}

my @embeds = parse_embed('parts/embed.fnc', $api_fnc);

# Look for %include lines in the ppport.h generator
my $PPPort = "$main_dir/PPPort_pm.PL";
open F, "<", $PPPort or die "Can't open $PPPort: $!";

my @no_parameters;
while (<F>) {
    next unless/^%include (\w+)/;
    my $implementation = parse_partspec("$main_dir/parts/inc/$1")->{'implementation'};

    # Find no-parameter entries using __UNDEFINED__ that aren't in the other.
    # We know these are provided.
    while ($implementation =~ /^__UNDEFINED__\s+(\w+)\s/mg) {
        push @no_parameters, $1 unless grep { $1 eq $_->{'name'} } @embeds;
    }
}

# Repeat, but look for ones that are 'provided' that don't use __UNDEFINED__
seek F, 0, 0;
while (<F>) {
    next unless/^%include (\w+)/;
    my $implementation = parse_partspec("$main_dir/parts/inc/$1")->{'implementation'};

    while ($implementation =~ /^#\s*define\s+(\w+)\s/mg) {
        next if grep { $1 eq $_ } @no_parameters;
        next if grep { $1 eq $_->{'name'} } @embeds;
        next unless grep { $1 eq $_ } @provided;
        push @no_parameters, $1;
    }
}

my @out = 'Am|void|sv_magic_portable|NN SV* sv|NULLOK SV* obj|int how|NULLOK const char* name|I32 namlen';
push @out, @non_public_provided;
push @out, map { "Amn|void|$_" } @no_parameters;

@out = sort sort_api_lines @out;

my $out = "parts/ppport.fnc";
open OUT, ">", $out or die "Could open '$out' for writing: $!";

print OUT <<EOF;
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:
:  !!!! Do NOT edit this file directly! -- Edit devel/mkppport_fnc.pl instead. !!!!
:
:  Perl/Pollution/Portability
:
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:
:  Version 3.x, Copyright (C) 2004-2013, Marcus Holland-Moritz.
:  Version 2.x, Copyright (C) 2001, Paul Marquess.
:  Version 1.x, Copyright (C) 1999, Kenneth Albanowski.
:
:  This program is free software; you can redistribute it and/or
:  modify it under the same terms as Perl itself.
:
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:
: This file lists all API functions/macros that are provided purely
: by Devel::PPPort, or that are not public.  It is in the same format as the
: F<embed.fnc> that ships with the Perl source code.
:
: Since these are used only to provide the argument types, it's ok to have the
: return value be void for some where it's an issues

EOF

print OUT join "\n", @out;
print OUT "\n";

close OUT;
