#! /usr/local/bin/perl -w

use v5.6.0;
use base Demo;
no warnings 'redefine';

my %z1 :Multi(method?maybe);
my %z2 :Multi(method,maybe);
my %z3 :Multi(qw(method,maybe));
my %z4 :Multi(qw(method maybe));
my %z5 :Multi('method','maybe');

sub foo :Demo(till=>ears=>are=>bleeding) {}
sub foo :Demo(['till','ears','are','bleeding']) {}
sub foo :Demo(qw/till ears are bleeding/) {}
sub foo :Demo(till,ears,are,bleeding) {}

sub foo :Demo(my,ears,are,bleeding) {}
sub foo :Demo(my=>ears=>are=>bleeding) {}
sub foo :Demo(qw/my, ears, are, bleeding/) {}
sub foo :Demo(qw/my ears are bleeding) {}
