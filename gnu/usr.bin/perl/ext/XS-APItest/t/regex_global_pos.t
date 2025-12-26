#!perl

use v5.40;

use Test::More;

use XS::APItest;

# get
{
    my $sv = "hello";
    is(sv_regex_global_pos_get($sv), undef, 'pos_get before setting');

    $sv =~ m/.../gc;
    is(sv_regex_global_pos_get($sv), 3, 'pos_get after successful match');
}

# set
{
    my $sv = "hello";
    sv_regex_global_pos_set($sv, 2);
    is(pos($sv), 2, 'pos() after pos_set');

    $sv =~ m/(.)/gc;
    is($1, "l", 'regexp match after pos_set');
    is(pos($sv), 3, 'pos() updated after match');

    sv_regex_global_pos_set($sv, -1);
    is(pos($sv), 4, 'pos() after pos_set to -1');
    $sv =~ m/(.)/gc;
    is($1, "o", 'regexp match after pos_set to -1');

    sv_regex_global_pos_clear($sv);
    $sv =~ m/(.)/gc;
    is($1, "h", 'regexp match after pos clear');
}

# characters vs bytes
{
    use utf8;
    my $sv = "café here";

    # pos should be 5 characters even though it is 6 bytes
    $sv =~ m/ /gc;
    is(sv_regex_global_pos_get($sv), 5, 'pos_get returns count in chars');
    # can query position in bytes directly
    is(sv_regex_global_pos_get($sv, SV_POSBYTES), 6, 'pos_get with SV_POSBYTES returns count in bytes');

    pos($sv) = 0;
    sv_regex_global_pos_set($sv, 4);
    $sv =~ m/(.)/gc;
    is($1, " ", 'regexp match after pos_set in chars');

    pos($sv) = 0;
    sv_regex_global_pos_set($sv, 5, SV_POSBYTES);
    $sv =~ m/(.)/gc;
    is($1, " ", 'regexp match after pos_set in bytes');
}

done_testing;
