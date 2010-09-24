#!/usr/bin/perl -w
#
# man-options.t -- Additional tests for Pod::Man options.
#
# Copyright 2002, 2004, 2006, 2008, 2009 Russ Allbery <rra@stanford.edu>
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

use Test::More;

# UTF-8 support requires Perl 5.8 or later.
BEGIN {
    if ($] < 5.008) {
        plan skip_all => 'Perl 5.8 required for UTF-8 support';
    } else {
        plan tests => 7;
    }
}
BEGIN { use_ok ('Pod::Man') }

# Force UTF-8 on all relevant file handles.  Do this inside eval in case the
# encoding parameter doesn't work.
eval { binmode (\*DATA, ':encoding(utf-8)') };
eval { binmode (\*STDOUT, ':encoding(utf-8)') };
my $builder = Test::More->builder;
eval { binmode ($builder->output, ':encoding(utf-8)') };
eval { binmode ($builder->failure_output, ':encoding(utf-8)') };

my $n = 1;
while (<DATA>) {
    my %options;
    next until $_ eq "###\n";
    while (<DATA>) {
        last if $_ eq "###\n";
        my ($option, $value) = split;
        $options{$option} = $value;
    }
    open (TMP, '> tmp.pod') or die "Cannot create tmp.pod: $!\n";
    eval { binmode (\*TMP, ':encoding(utf-8)') };
    print TMP "=encoding utf-8\n\n";
    while (<DATA>) {
        last if $_ eq "###\n";
        print TMP $_;
    }
    close TMP;
    my $parser = Pod::Man->new (%options);
    isa_ok ($parser, 'Pod::Man', 'Parser object');
    open (OUT, '> out.tmp') or die "Cannot create out.tmp: $!\n";
    $parser->parse_from_file ('tmp.pod', \*OUT);
    close OUT;
    my $accents = 0;
    open (TMP, 'out.tmp') or die "Cannot open out.tmp: $!\n";
    eval { binmode (\*TMP, ':encoding(utf-8)') };
    while (<TMP>) {
        $accents = 1 if /Accent mark definitions/;
        last if /^\.nh/;
    }
    my $output;
    {
        local $/;
        $output = <TMP>;
    }
    close TMP;
    1 while unlink ('tmp.pod', 'out.tmp');
    if ($options{utf8}) {
        ok (!$accents, "Saw no accent definitions for test $n");
    } else {
        ok ($accents, "Saw accent definitions for test $n");
    }
    my $expected = '';
    while (<DATA>) {
        last if $_ eq "###\n";
        $expected .= $_;
    }
    is ($output, $expected, "Output correct for test $n");
    $n++;
}

# Below the marker are bits of POD and corresponding expected text output.
# This is used to test specific features or problems with Pod::Man.  The
# input and output are separated by lines containing only ###.

__DATA__

###
utf8 1
###
=head1 BEYONCÉ

Beyoncé!  Beyoncé!  Beyoncé!!

    Beyoncé!  Beyoncé!
      Beyoncé!  Beyoncé!
        Beyoncé!  Beyoncé!

Older versions did not convert Beyoncé in verbatim.
###
.SH "BEYONCÉ"
.IX Header "BEYONCÉ"
Beyoncé!  Beyoncé!  Beyoncé!!
.PP
.Vb 3
\&    Beyoncé!  Beyoncé!
\&      Beyoncé!  Beyoncé!
\&        Beyoncé!  Beyoncé!
.Ve
.PP
Older versions did not convert Beyoncé in verbatim.
###

###
utf8 1
###
=head1 SE<lt>E<gt> output with UTF-8

This is S<non-breaking output>.
###
.SH "S<> output with UTF\-8"
.IX Header "S<> output with UTF-8"
This is non-breaking output.
###
