#!/usr/bin/perl
use strict;
use warnings;

use Pod::PlainText;
use Test::More;

my $invalid = q{
=head1 One

=begin foo

Foo
};

my $valid = q{
=head1 Two

=begin bar

Bar

=end bar

=head1 Three
};


my $parser = Pod::PlainText->new;

my $out = '';
open my $out_fh, '>', \$out or die "Couldn't open out: $!";

{
    open my $fh, '<', \$invalid or die "Couldn't open invalid: $!";
    $parser->parse_from_filehandle($fh, $out_fh);
    close $fh;
}

{
    open my $fh, '<', \$valid or die "Couldn't open valid: $!";
    $parser->parse_from_filehandle($fh, $out_fh);
    close $fh;
}

close $out_fh;


is $out, "One\nTwo\nThree\n", "Correctly parsed valid document";

done_testing;
