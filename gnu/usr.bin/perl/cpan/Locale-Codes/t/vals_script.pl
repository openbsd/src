#!/usr/bin/perl
# Copyright (c) 2016-2018 Sullivan Beck. All rights reserved.
# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.

use warnings;
use strict;

$::tests = '';
$::tests = "

2code
Phoenician
   phnx

2code
Phoenician
num
   115

2name
Phnx
   Phoenician

2name
phnx
   Phoenician

2name
115
num
   Phoenician

code2code
Phnx
alpha
num
   115

all_codes
2
   ~
   Adlm
   Afak

all_names
2
   ~
   Adlam
   Afaka

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

