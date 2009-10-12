#!/usr/bin/perl -w
#
# man-options.t -- Additional tests for Pod::Man options.
#
# Copyright 2002, 2004, 2006, 2008 Russ Allbery <rra@stanford.edu>
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
    print "1..7\n";
}

END {
    print "not ok 1\n" unless $loaded;
}

use Pod::Man;

# Redirect stderr to a file.
sub stderr_save {
    open (OLDERR, '>&STDERR') or die "Can't dup STDERR: $!\n";
    open (STDERR, '> out.err') or die "Can't redirect STDERR: $!\n";
}

# Restore stderr.
sub stderr_restore {
    close STDERR;
    open (STDERR, '>&OLDERR') or die "Can't dup STDERR: $!\n";
    close OLDERR;
}

$loaded = 1;
print "ok 1\n";

my $n = 2;
while (<DATA>) {
    my %options;
    next until $_ eq "###\n";
    while (<DATA>) {
        last if $_ eq "###\n";
        my ($option, $value) = split;
        $options{$option} = $value;
    }
    open (TMP, '> tmp.pod') or die "Cannot create tmp.pod: $!\n";
    while (<DATA>) {
        last if $_ eq "###\n";
        print TMP $_;
    }
    close TMP;
    my $parser = Pod::Man->new (%options) or die "Cannot create parser\n";
    open (OUT, '> out.tmp') or die "Cannot create out.tmp: $!\n";
    stderr_save;
    $parser->parse_from_file ('tmp.pod', \*OUT);
    stderr_restore;
    close OUT;
    my $accents = 0;
    open (TMP, 'out.tmp') or die "Cannot open out.tmp: $!\n";
    while (<TMP>) {
        last if /^\.nh/;
    }
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
    open (ERR, 'out.err') or die "Cannot open out.err: $!\n";
    my $errors;
    {
        local $/;
        $errors = <ERR>;
    }
    close ERR;
    unlink ('out.err');
    $expected = '';
    while (<DATA>) {
        last if $_ eq "###\n";
        $expected .= $_;
    }
    if ($errors eq $expected) {
        print "ok $n\n";
    } else {
        print "not ok $n\n";
        print "Expected errors:\n    ${expected}Errors:\n    $errors";
    }
    $n++;
}

# Below the marker are bits of POD and corresponding expected text output.
# This is used to test specific features or problems with Pod::Man.  The
# input and output are separated by lines containing only ###.

__DATA__

###
fixed CR
fixedbold CY
fixeditalic CW
fixedbolditalic CX
###
=head1 FIXED FONTS

C<foo B<bar I<baz>> I<bay>>
###
.SH "FIXED FONTS"
.IX Header "FIXED FONTS"
\&\f(CR\*(C`foo \f(CYbar \f(CXbaz\f(CY\f(CR \f(CWbay\f(CR\*(C'\fR
###
###

###
###
=over 4

=item Foo

Bar.

=head1 NEXT
###
.IP "Foo" 4
.IX Item "Foo"
Bar.
.SH "NEXT"
.IX Header "NEXT"
.SH "POD ERRORS"
.IX Header "POD ERRORS"
Hey! \fBThe above document had some coding errors, which are explained below:\fR
.IP "Around line 7:" 4
.IX Item "Around line 7:"
You forgot a '=back' before '=head1'
###
###

###
stderr 1
###
=over 4

=item Foo

Bar.

=head1 NEXT
###
.IP "Foo" 4
.IX Item "Foo"
Bar.
.SH "NEXT"
.IX Header "NEXT"
###
tmp.pod around line 7: You forgot a '=back' before '=head1'
###
