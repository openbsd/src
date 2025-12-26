#!./perl -w
# t/qquote.t - Test qquote()

use strict;
use warnings;

use Test::More tests => 16;
use Data::Dumper;

{
    my $warning = '';
    local $SIG{__WARN__} = sub { $warning = $_[0] };

    my $str = Data::Dumper::qquote("");
    is($str,   q{""}, q{qquote("") returned ""});
    is($warning, "",  q{qquote("") did not warn});
}

{
    my $warning = '';
    local $SIG{__WARN__} = sub { $warning = $_[0] };

    my $str = Data::Dumper::qquote();
    is($str,   q{""}, q{qquote() returned ""});
    is($warning, "",  q{qquote() did not warn});
}

{
    my $warning = '';
    local $SIG{__WARN__} = sub { $warning = $_[0] };

    my $str = Data::Dumper::qquote(undef);
    is($str,   q{""}, q{qquote(undef) returned ""});
    is($warning, "",  q{qquote(undef) did not warn});
}

{
    my $warning = '';
    local $SIG{__WARN__} = sub { $warning = $_[0] };

    my $str = Data::Dumper::qquote("simple");
    is($str,   q{"simple"}, q{qquote("simple") returned "simple"});
    is($warning, "",        q{qquote("simple") did not warn});
}

{
    my $warning = '';
    local $SIG{__WARN__} = sub { $warning = $_[0] };

    my $str = Data::Dumper::qquote(q{check 'single' quote});
    is($str,   q{"check 'single' quote"}, q{qquote('single') returned correctly});
    is($warning, "", q{qquote('single') did not warn});
}

{
    my $warning = '';
    local $SIG{__WARN__} = sub { $warning = $_[0] };

    my $str = Data::Dumper::qquote(q{check "double" quote});
    is($str,     q{"check \"double\" quote"}, q{qquote("double") returned correctly});
    is($warning, "", 'qquote(undef) did not warn');
}

{
    my $warning = '';
    local $SIG{__WARN__} = sub { $warning = $_[0] };

    my $str = Data::Dumper::qquote(qq{check \a quote});
    is($str,     q{"check \a quote"}, q{qquote("\a") returned correctly});
    is($warning, "", 'qquote(undef) did not warn');
}

SKIP: {
    skip "ASCII-centric test", 2, unless ord "A" == 65;
    my $warning = '';
    local $SIG{__WARN__} = sub { $warning = $_[0] };

    my $str = Data::Dumper::qquote(qq{check \cg quote});
    is($str,     q{"check \a quote"}, q{qquote("\cg") returned correctly});
    is($warning, "", 'qquote(undef) did not warn');
}
