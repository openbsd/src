#!/usr/bin/perl -w
# $Id: man.t,v 1.4 2003/01/05 06:31:52 eagle Exp $
#
# man.t -- Additional specialized tests for Pod::Man.
#
# Copyright 2002, 2003 by Russ Allbery <rra@stanford.edu>
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

use Pod::Man;

$loaded = 1;
print "ok 1\n";

my $n = 2;
while (<DATA>) {
    next until $_ eq "###\n";
    open (TMP, '> tmp.pod') or die "Cannot create tmp.pod: $!\n";
    while (<DATA>) {
        last if $_ eq "###\n";
        print TMP $_;
    }
    close TMP;
    my $parser = Pod::Man->new or die "Cannot create parser\n";
    $parser->parse_from_file ('tmp.pod', 'out.tmp');
    open (TMP, 'out.tmp') or die "Cannot open out.tmp: $!\n";
    while (<TMP>) { last if /^\.TH/ }
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

# Below the marker are bits of POD and corresponding expected nroff output.
# This is used to test specific features or problems with Pod::Man.  The input
# and output are separated by lines containing only ###.

__DATA__

###
=head1 NAME

gcc - GNU project C and C++ compiler

=head1 C++ NOTES

Other mentions of C++.
###
.SH "NAME"
gcc \- GNU project C and C++ compiler
.SH "\*(C+ NOTES"
.IX Header " NOTES"
Other mentions of \*(C+.
###

###
=head1 PERIODS

This C<.> should be quoted.
###
.SH "PERIODS"
.IX Header "PERIODS"
This \f(CW\*(C`.\*(C'\fR should be quoted.
###

###
=over 4

=item *

A bullet.

=item    *

Another bullet.

=item * Not a bullet.

=back
###
.IP "\(bu" 4
A bullet.
.IP "\(bu" 4
Another bullet.
.IP "* Not a bullet." 4
.IX Item "Not a bullet."
###

###
=over 4

=item foo

Not a bullet.

=item *

Also not a bullet.

=back
###
.IP "foo" 4
.IX Item "foo"
Not a bullet.
.IP "*" 4
Also not a bullet.
###
