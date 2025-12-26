#!/usr/bin/perl -w

# This script tests %ExtUtils::MakeMaker::Recognized_Att_Keys;

use strict;
use Test::More;

# We don’t need to test all parameters; just enough to verify that the
# mechanism is working.  This list is somewhat random, but it works.

my @supported = qw(
 ABSTRACT_FROM
 AUTHOR
 BUILD_REQUIRES
 clean
 dist
 DISTNAME
 DISTVNAME
 LIBS
 MAN3PODS
 META_MERGE
 MIN_PERL_VERSION
 NAME
 PL_FILES
 PREREQ_PM
 VERSION
 VERSION_FROM
);

my @unsupported = qw(
 WIBBLE
 wump
);

plan tests => @supported+@unsupported;

use ExtUtils::MakeMaker ();

for (@supported) {
    ok exists $ExtUtils::MakeMaker::Recognized_Att_Keys{$_},
      "EUMM says it supports param '$_'";
}
for (@unsupported) {
    ok !exists $ExtUtils::MakeMaker::Recognized_Att_Keys{$_},
        "EUMM claims not to support param '$_'";
}
