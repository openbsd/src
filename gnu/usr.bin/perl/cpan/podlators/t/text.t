#!/usr/bin/perl -w
#
# text.t -- Additional specialized tests for Pod::Text.
#
# Copyright 2002, 2004, 2006, 2007, 2008, 2009, 2012
#     Russ Allbery <rra@stanford.edu>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

BEGIN {
    chdir 't' if -d 't';
    if ($ENV{PERL_CORE}) {
        @INC = '../lib';
    }
    unshift (@INC, '../blib/lib');
    $| = 1;
}

use strict;

use Pod::Simple;
use Test::More tests => 9;
BEGIN { use_ok ('Pod::Text') }

my $parser = Pod::Text->new;
isa_ok ($parser, 'Pod::Text', 'Parser object');
my $n = 1;
while (<DATA>) {
    next until $_ eq "###\n";
    open (TMP, "> tmp$$.pod") or die "Cannot create tmp$$.pod: $!\n";
    while (<DATA>) {
        last if $_ eq "###\n";
        print TMP $_;
    }
    close TMP;
    open (OUT, "> out$$.tmp") or die "Cannot create out$$.tmp: $!\n";
    $parser->parse_from_file ("tmp$$.pod", \*OUT);
    close OUT;
    open (TMP, "out$$.tmp") or die "Cannot open out$$.tmp: $!\n";
    my $output;
    {
        local $/;
        $output = <TMP>;
    }
    close TMP;
    1 while unlink ("tmp$$.pod", "out$$.tmp");
    my $expected = '';
    while (<DATA>) {
        last if $_ eq "###\n";
        $expected .= $_;
    }
    is ($output, $expected, "Output correct for test $n");
    $n++;
}

# Below the marker are bits of POD and corresponding expected text output.
# This is used to test specific features or problems with Pod::Text.  The
# input and output are separated by lines containing only ###.

__DATA__

###
=head1 PERIODS

This C<.> should be quoted.
###
PERIODS
    This "." should be quoted.

###

###
=head1 CE<lt>E<gt> WITH SPACES

What does C<<  this.  >> end up looking like?
###
C<> WITH SPACES
    What does "this." end up looking like?

###

###
=head1 Test of SE<lt>E<gt>

This is some S<  > whitespace.
###
Test of S<>
    This is some    whitespace.

###

###
=head1 Test of =for

=for comment
This won't be seen.

Yes.

=for text
This should be seen.

=for TEXT As should this.

=for man
But this shouldn't.

Some more text.
###
Test of =for
    Yes.

This should be seen.
As should this.
    Some more text.

###

###
=pod

text

  line1
  
  line3
###
    text

      line1
  
      line3

###

###
=head1 LINK TO URL

This is a L<link|http://www.example.com/> to a URL.
###
LINK TO URL
    This is a link <http://www.example.com/> to a URL.

###

###
=head1 RT LINK

L<[perl #12345]|https://rt.cpan.org/12345>
###
RT LINK
    [perl #12345] <https://rt.cpan.org/12345>

###
