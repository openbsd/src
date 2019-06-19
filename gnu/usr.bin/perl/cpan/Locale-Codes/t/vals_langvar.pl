#!/usr/bin/perl
# Copyright (c) 2016-2018 Sullivan Beck. All rights reserved.
# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.

use warnings;
use strict;

$::tests = '';

$::tests = "

2code
Eastern Armenian
   arevela

2name
arevela
   Eastern Armenian

code2code
arevela
alpha
alpha
   arevela

all_codes
2
   ~
   1606nict
   1694acad

all_names
2
   ~
   \"Academic\" (\"governmental\") variant of Belarusian as codified in 1959
   ALA-LC Romanization, 1997 edition

";

1;
# Local Variables:
# mode: cperl
# indent-tabs-mode: nil
# cperl-indent-level: 3
# cperl-continued-statement-offset: 2
# cperl-continued-brace-offset: 0
# cperl-brace-offset: 0
# cperl-brace-imaginary-offset: 0
# cperl-label-offset: 0
# End:

