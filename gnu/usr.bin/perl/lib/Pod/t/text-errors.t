#!/usr/bin/perl -w
# $Id: text-errors.t,v 1.1 2002/01/01 02:41:53 eagle Exp $
#
# texterrs.t -- Error tests for Pod::Text.
#
# Copyright 2001 by Russ Allbery <rra@stanford.edu>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

BEGIN {
    chdir 't' if -d 't';
    if ($ENV{PERL_CORE}) {
        @INC = '../lib';
    } else {
        unshift (@INC, '../blib/lib');
    }
    unshift (@INC, '../blib/lib');
    $| = 1;
    print "1..5\n";
}

END {
    print "not ok 1\n" unless $loaded;
}

use Pod::Text;

$loaded = 1;
print "ok 1\n";

# Hard-code a few values to try to get reproducible results.
$ENV{COLUMNS} = 80;
$ENV{TERM} = 'xterm';
$ENV{TERMCAP} = 'xterm:co=80:do=^J:md=\E[1m:us=\E[4m:me=\E[m';

# Set default options to match those of pod2man and pod2text.
my %options = (sentence => 0);

# Capture warnings for inspection.
my $warnings = '';
$SIG{__WARN__} = sub { $warnings .= $_[0] };

# Run a single test, given some POD to parse and the warning messages that are
# expected.  Any formatted output is ignored; only warning messages are
# checked.  Writes the POD to a temporary file since that's the easiest way to
# interact with Pod::Parser.
sub test_error {
    my ($pod, $expected) = @_;
    open (TMP, '> tmp.pod') or die "Cannot create tmp.pod: $!\n";
    print TMP $pod;
    close TMP;
    my $parser = Pod::Text->new (%options);
    return unless $parser;
    $warnings = '';
    $parser->parse_from_file ('tmp.pod', 'out.tmp');
    unlink ('tmp.pod', 'out.tmp');
    if ($warnings eq $expected) {
        return 1;
    } else {
        print "  # '$warnings'\n  # '$expected'\n";
        return 0;
    }
}

# The actual tests.
my @tests = (
    [ "=head1 a E<0x2028> b\n"
        => "tmp.pod:1: Unknown escape: E<0x2028>\n" ],
    [ "=head1 a Y<0x2028> b\n"
        => "tmp.pod:1: Unknown formatting code: Y<0x2028>\n" ],
    [ "=head1 TEST\n\n=command args\n"
        => "tmp.pod:3: Unknown command paragraph: =command args\n" ],
    [ "=head1 TEST\n\n  Foo bar\n\n=back\n"
        => "tmp.pod:5: Unmatched =back\n" ]
);
my $n = 2;
for (@tests) {
    print (test_error ($$_[0], $$_[1]) ? "ok $n\n" : "not ok $n\n");
    $n++;
}
