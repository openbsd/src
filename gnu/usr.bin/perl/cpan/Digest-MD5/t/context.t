#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use Digest::MD5;

foreach my $string ( map { 'a' x $_ }
    1..17,
    31..33,
    64..65,
    127..129,
    191..193,
    1023..1025,
    2047..2049,
) {
    my $expect = do {
        my $ctx = Digest::MD5->new;
        $ctx->add($string);
        $ctx->add($string);
        $ctx->add($string);
        $ctx->hexdigest;
    };

    my $got = do {
        my $ctx1 = Digest::MD5->new;
        $ctx1->add($string);

        my $ctx2 = Digest::MD5->new;
        $ctx2->context( $ctx1->context );
        $ctx2->add($string);

        my $ctx3 = Digest::MD5->new;
        $ctx3->context( $ctx2->context );
        $ctx3->add($string);

        $ctx3->hexdigest;
    };

    is $got, $expect, length($string) . " saved context";
}

done_testing;
