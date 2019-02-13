# -*- perl -*-
use strict;
use Pod::Html qw( anchorify );
use Test::More tests => 1;

my @filedata;
{
    local $/ = '';
    @filedata = <DATA>;
}

my (@poddata, $i, $j);
for ($i = 0, $j = -1; $i <= $#filedata; $i++) {
    $j++ if ($filedata[$i] =~ /^\s*=head[1-6]/);
    if ($j >= 0) {
        $poddata[$j]  = "" unless defined $poddata[$j];
        $poddata[$j] .= "\n$filedata[$i]" if $j >= 0;
    }
}

my %heads = ();
foreach $i (0..$#poddata) {
    $heads{anchorify($1)} = 1 if $poddata[$i] =~ /=head[1-6]\s+(.*)/;
}
my %expected = map { $_ => 1 } qw(
    NAME
    DESCRIPTION
    Subroutine
    Error
    Method
    Has_A_Wordspace
    HasTrailingWordspace
    HasLeadingWordspace
    Has_Extra_InternalWordspace
    Has_Quotes
    Has_QuestionMark
    Has_Hyphen_And_Space
);
is_deeply(
    \%heads,
    \%expected,
    "Got expected POD heads"
);

__DATA__
=head1 NAME

anchorify - Test C<Pod::Html::anchorify()>

=head1 DESCRIPTION

alpha

=head2 Subroutine

beta

=head3 Error

gamma

=head4 Method

delta

=head4 Has A Wordspace

delta

=head4 HasTrailingWordspace  

epsilon

=head4    HasLeadingWordspace

zeta

=head4 Has	Extra  InternalWordspace

eta

=head4 Has"Quotes"

theta

=head4 Has?QuestionMark

iota

=head4 Has-Hyphen And Space

kappa

=cut

__END__
