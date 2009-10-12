#!/usr/bin/perl -w
#
# termcap.t -- Additional specialized tests for Pod::Text::Termcap.
#
# Copyright 2002, 2004, 2006 by Russ Allbery <rra@stanford.edu>
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
    print "1..2\n";
}

END {
    print "not ok 1\n" unless $loaded;
}

# Hard-code a few values to try to get reproducible results.
$ENV{COLUMNS} = 80;
$ENV{TERM} = 'xterm';
$ENV{TERMCAP} = 'xterm:co=80:do=^J:md=\E[1m:us=\E[4m:me=\E[m';

use Pod::Text::Termcap;

$loaded = 1;
print "ok 1\n";

my $parser = Pod::Text::Termcap->new or die "Cannot create parser\n";
my $n = 2;
while (<DATA>) {
    next until $_ eq "###\n";
    open (TMP, '> tmp.pod') or die "Cannot create tmp.pod: $!\n";
    while (<DATA>) {
        last if $_ eq "###\n";
        print TMP $_;
    }
    close TMP;
    open (OUT, '> out.tmp') or die "Cannot create out.tmp: $!\n";
    $parser->parse_from_file ('tmp.pod', \*OUT);
    close OUT;
    open (TMP, 'out.tmp') or die "Cannot open out.tmp: $!\n";
    my $output;
    {
        local $/;
        $output = <TMP>;
    }
    close TMP;
    unlink ('tmp.pod', 'out.tmp');
    my $expected = '';
    while (<DATA>) {
        last if $_ eq "###\n";
        $expected .= $_;
    }
    if ($output eq $expected) {
        print "ok $n\n";
    } else {
        print "not ok $n\n";
        print "Expected\n========\n$expected\nOutput\n======\n$output\n";
    }
    $n++;
}

# Below the marker are bits of POD and corresponding expected output.  This is
# used to test specific features or problems with Pod::Text::Termcap.  The
# input and output are separated by lines containing only ###.

__DATA__

###
=head1 WRAPPING

B<I<Do>> I<B<not>> B<I<include>> B<I<formatting codes when>> B<I<wrapping>>.
###
[1mWRAPPING[m
    [1m[4mDo[m[m [4m[1mnot[m[m [1m[4minclude[m[m [1m[4mformatting codes when[m[m [1m[4mwrapping[m[m.

###
