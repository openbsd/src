#!/usr/bin/perl -w
#
# text-utf8.t -- Test Pod::Text with UTF-8 input.
#
# Copyright 2002, 2004, 2006, 2007, 2008 by Russ Allbery <rra@stanford.edu>
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
    print "1..3\n";

    # UTF-8 support requires Perl 5.8 or later.
    if ($] < 5.008) {
        my $n;
        for $n (1..3) {
            print "ok $n # skip -- Perl 5.8 required for UTF-8 support\n";
        }
        exit;
    }
}

END {
    print "not ok 1\n" unless $loaded;
}

use Pod::Text;

$loaded = 1;
print "ok 1\n";

my $parser = Pod::Text->new or die "Cannot create parser\n";
my $n = 2;
eval { binmode (\*DATA, ':encoding(utf-8)') };
eval { binmode (\*STDOUT, ':encoding(utf-8)') };
while (<DATA>) {
    next until $_ eq "###\n";
    open (TMP, '> tmp.pod') or die "Cannot create tmp.pod: $!\n";
    eval { binmode (\*TMP, ':encoding(utf-8)') };
    print TMP "=encoding UTF-8\n\n";
    while (<DATA>) {
        last if $_ eq "###\n";
        print TMP $_;
    }
    close TMP;
    open (OUT, '> out.tmp') or die "Cannot create out.tmp: $!\n";
    $parser->parse_from_file ('tmp.pod', \*OUT);
    close OUT;
    open (TMP, 'out.tmp') or die "Cannot open out.tmp: $!\n";
    eval { binmode (\*TMP, ':encoding(utf-8)') };
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

# Below the marker are bits of POD and corresponding expected text output.
# This is used to test specific features or problems with Pod::Text.  The
# input and output are separated by lines containing only ###.

__DATA__

###
=head1 Test of SE<lt>E<gt>

This is S<some whitespace>.
###
Test of S<>
    This is some whitespace.

###

###
=head1 I can eat glass

=over 4

=item Esperanto

Mi povas manĝi vitron, ĝi ne damaĝas min.

=item Braille

⠊⠀⠉⠁⠝⠀⠑⠁⠞⠀⠛⠇⠁⠎⠎⠀⠁⠝⠙⠀⠊⠞⠀⠙⠕⠑⠎⠝⠞⠀⠓⠥⠗⠞⠀⠍⠑

=item Hindi

मैं काँच खा सकता हूँ और मुझे उससे कोई चोट नहीं पहुंचती.

=back

See L<http://www.columbia.edu/kermit/utf8.html>
###
I can eat glass
    Esperanto
        Mi povas manĝi vitron, ĝi ne damaĝas min.

    Braille
        ⠊⠀⠉⠁⠝⠀⠑⠁⠞⠀⠛⠇⠁⠎⠎⠀⠁⠝⠙⠀⠊⠞⠀⠙⠕⠑⠎⠝⠞⠀⠓⠥⠗⠞⠀⠍⠑

    Hindi
        मैं काँच खा सकता हूँ और मुझे उससे कोई चोट नहीं पहुंचती.

    See <http://www.columbia.edu/kermit/utf8.html>

###
