#!/usr/bin/perl
# Copyright (c) 2016-2018 Sullivan Beck. All rights reserved.
# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.

use warnings;
use strict;

$::tests = '';

$::tests = "

2code
Apache languages
   ~
   apa

2name
apa
   Apache languages

code2code
apa
alpha
alpha
   apa

all_codes
2
   ~
   aav
   afa

all_names
2
   ~
   Afro-Asiatic languages
   Alacalufan languages

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

