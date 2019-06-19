#!/usr/bin/perl -w
#
# Additional tests for Pod::Text options.
#
# Copyright 2002, 2004, 2006, 2008, 2009, 2012, 2013, 2015
#     Russ Allbery <rra@cpan.org>
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

use Test::More tests => 37;
BEGIN { use_ok ('Pod::Text') }

# Redirect stderr to a file.
sub stderr_save {
    open (OLDERR, '>&STDERR') or die "Can't dup STDERR: $!\n";
    open (STDERR, "> out$$.err") or die "Can't redirect STDERR: $!\n";
}

# Restore stderr.
sub stderr_restore {
    close STDERR;
    open (STDERR, '>&OLDERR') or die "Can't dup STDERR: $!\n";
    close OLDERR;
}

my $n = 1;
while (<DATA>) {
    my %options;
    next until $_ eq "###\n";
    while (<DATA>) {
        last if $_ eq "###\n";
        my ($option, $value) = split;
        $options{$option} = $value;
    }
    open (TMP, "> tmp$$.pod") or die "Cannot create tmp$$.pod: $!\n";
    while (<DATA>) {
        last if $_ eq "###\n";
        print TMP $_;
    }
    close TMP;
    my $parser = Pod::Text->new (%options);
    isa_ok ($parser, 'Pod::Text', 'Parser object');
    open (OUT, "> out$$.tmp") or die "Cannot create out$$.tmp: $!\n";
    stderr_save;
    eval { $parser->parse_from_file ("tmp$$.pod", \*OUT) };
    my $exception = $@;
    stderr_restore;
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
    open (ERR, "out$$.err") or die "Cannot open out$$.err: $!\n";
    my $errors;
    {
        local $/;
        $errors = <ERR>;
    }
    close ERR;
    $errors =~ s/\Qtmp$$.pod/tmp.pod/g;
    1 while unlink ("out$$.err");
    if ($exception) {
        $exception =~ s/ at .*//;
        $errors .= "EXCEPTION: $exception";
    }
    $expected = '';
    while (<DATA>) {
        last if $_ eq "###\n";
        $expected .= $_;
    }
    is ($errors, $expected, "Errors correct for test $n");
    $n++;
}

# Below the marker are bits of POD and corresponding expected text output.
# This is used to test specific features or problems with Pod::Text.  The
# options, input, output, and errors are separated by lines containing only
# ###.

__DATA__

###
alt 1
###
=head1 SAMPLE

=over 4

=item F

Paragraph.

=item Bar

=item B

Paragraph.

=item Longer

Paragraph.

=back

###

==== SAMPLE ====

:   F   Paragraph.

:   Bar
:   B   Paragraph.

:   Longer
        Paragraph.

###
###

###
margin 4
###
=head1 SAMPLE

This is some body text that is long enough to be a paragraph that wraps,
thereby testing margins with wrapped paragraphs.

 This is some verbatim text.

=over 6

=item Test

This is a test of an indented paragraph.

This is another indented paragraph.

=back
###
    SAMPLE
        This is some body text that is long enough to be a paragraph that
        wraps, thereby testing margins with wrapped paragraphs.

         This is some verbatim text.

        Test  This is a test of an indented paragraph.

              This is another indented paragraph.

###
###

###
code 1
###
This is some random text.
This is more random text.

This is some random text.
This is more random text.

=head1 SAMPLE

This is POD.

=cut

This is more random text.
###
This is some random text.
This is more random text.

This is some random text.
This is more random text.

SAMPLE
    This is POD.


This is more random text.
###
###

###
sentence 1
###
=head1 EXAMPLE

Whitespace around C<<  this.  >> must be ignored per perlpodspec.  >>
needs to eat all of the space in front of it.

=cut
###
EXAMPLE
    Whitespace around "this." must be ignored per perlpodspec.  >> needs to
    eat all of the space in front of it.

###
###

###
###
=over 4

=item Foo

Bar.

=head1 NEXT
###
    Foo Bar.

NEXT
POD ERRORS
    Hey! The above document had some coding errors, which are explained
    below:

    Around line 7:
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
    Foo Bar.

NEXT
###
tmp.pod around line 7: You forgot a '=back' before '=head1'
###

###
nourls 1
###
=head1 URL suppression

L<anchor|http://www.example.com/>
###
URL suppression
    anchor

###
###

###
errors stderr
###
=over 4

=item Foo

Bar.

=head1 NEXT
###
    Foo Bar.

NEXT
###
tmp.pod around line 7: You forgot a '=back' before '=head1'
###

###
errors die
###
=over 4

=item Foo

Bar.

=head1 NEXT
###
    Foo Bar.

NEXT
###
tmp.pod around line 7: You forgot a '=back' before '=head1'
EXCEPTION: POD document had syntax errors
###

###
errors pod
###
=over 4

=item Foo

Bar.

=head1 NEXT
###
    Foo Bar.

NEXT
POD ERRORS
    Hey! The above document had some coding errors, which are explained
    below:

    Around line 7:
        You forgot a '=back' before '=head1'

###
###

###
errors none
###
=over 4

=item Foo

Bar.

=head1 NEXT
###
    Foo Bar.

NEXT
###
###

###
quotes <<<>>>
###
=head1 FOO C<BAR> BAZ

Foo C<bar> baz.
###
FOO <<<BAR>>> BAZ
    Foo <<<bar>>> baz.

###
###
