#!/usr/bin/perl
# Copyright (c) 2016-2018 Sullivan Beck. All rights reserved.
# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.

use warnings;
use strict;

$::tests = '';

$::tests = "
all_names
foo
2
   ~
   ERROR: _code: invalid codeset provided: foo

2name
zz
   ~
   ERROR: _code: code not in codeset: zz [alpha-2]

type
zz
   ~
   ERROR: type: invalid argument: zz

2name
aaa
numeric
   ~
   ERROR: _code: invalid numeric code: aaa

codeset
zz
   ~
   ERROR: codeset: invalid argument: zz

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

