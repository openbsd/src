#!/usr/bin/perl
# Copyright (c) 2016-2018 Sullivan Beck. All rights reserved.
# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.

use warnings;
use strict;

$::tests = '';

$::tests = "

2code
Canadian Dollar
   cad

2code
Belize Dollar
   bzd

2code
PULA
   bwp

2code
Riel
   khr

2code
Zimbabwe Dollar
   zwl

2name
KHR
   Riel

code2code
BZD
alpha
num
   084

2name
BOB
   Boliviano

2name
all
   Lek

2name
bnd
   Brunei Dollar

2name
bob
   Boliviano

2name
chf
   Swiss Franc

2name
cop
   Colombian Peso

2name
dkk
   Danish Krone

2name
fjd
   Fiji Dollar

2name
idr
   Rupiah

2name
mmk
   Kyat

2name
mvr
   Rufiyaa

2name
mwk
   Malawi Kwacha

2name
rub
   Russian Ruble

2name
zmw
   Zambian Kwacha

2name
zwl
   Zimbabwe Dollar

all_codes
2
   ~
   AED
   AFN

all_names
2
   ~
   ADB Unit of Account
   Afghani

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

