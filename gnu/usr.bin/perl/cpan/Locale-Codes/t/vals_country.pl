#!/usr/bin/perl
# Copyright (c) 2016-2018 Sullivan Beck. All rights reserved.
# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.

use warnings;
use strict;

$::tests = '';

$::tests = "
##################
# code2country

all_names
2
   ~
   Afghanistan
   Aland Islands

all_codes
2
   ~
   ad
   ae

all_names
retired
2
   ~
   Afghanistan
   Aland Islands

all_codes
retired
2
   ~
   ad
   ae

all_names
foo
2
   ~

all_codes
foo
2
   ~

2name
zz
   _undef_

2name
zz
alpha-2
   _undef_

2name
zz
alpha-3
   _undef_

2name
zz
numeric
   _undef_

2name
ja
   _undef_

2name
uk
   _undef_

2name
BO
   Bolivia (Plurinational State of)

2name
BO
alpha-2
   Bolivia (Plurinational State of)

2name
bol
alpha-3
   Bolivia (Plurinational State of)

2name
pk
   Pakistan

2name
sn
   Senegal

2name
us
   United States of America

2name
ad
   Andorra

2name
ad
alpha-2
   Andorra

2name
and
alpha-3
   Andorra

2name
020
numeric
   Andorra

2name
48
numeric
   Bahrain

2name
zw
   Zimbabwe

2name
gb
   United Kingdom of Great Britain and Northern Ireland

2name
kz
   Kazakhstan

2name
mo
   Macao

2name
tl
alpha-2
   Timor-Leste

2name
tls
alpha-3
   Timor-Leste

2name
626
numeric
   Timor-Leste

2name
BO
alpha-3
   _undef_

2name
BO
numeric
   _undef_

2name
ax
   Aland Islands

2name
ala
alpha-3
   Aland Islands

2name
248
numeric
   Aland Islands

2name
scg
alpha-3
   _undef_

2name
891
numeric
   _undef_

2name
rou
alpha-3
   Romania

2name
zr
   _undef_

2name
zr
retired
   Zaire

2name
jp
alpha-2
not_retired
other_arg
   Japan

2name
jp
_blank_
   Japan

2name
jp
alpha-15
   _undef_

2name
jp
alpha-2
retired
   Japan

2name
z0
alpha-2
retired
   _undef_

##################
# country2code

2code
kazakhstan
   kz

2code
kazakstan
   kz

2code
macao
   mo

2code
macau
   mo

2code
japan
   jp

2code
Japan
   jp

2code
United States
   us

2code
United Kingdom
   gb

2code
Andorra
   ad

2code
Zimbabwe
   zw

2code
Iran
   ir

2code
North Korea
   kp

2code
South Korea
   kr

2code
Libya
   ly

2code
Syrian Arab Republic
   sy

2code
Svalbard
   _undef_

2code
Jan Mayen
   _undef_

2code
USA
   us

2code
United States of America
   us

2code
Great Britain
   gb

2code
Burma
   mm

2code
French Southern and Antarctic Lands
   tf

2code
Aland Islands
   ax

2code
Yugoslavia
   _undef_

2code
Serbia and Montenegro
   _undef_

2code
East Timor
   tl

2code
Zaire
   _undef_

2code
Zaire
retired
   zr

2code
Congo, The Democratic Republic of the
   cd

2code
Congo, The Democratic Republic of the
alpha-3
   cod

2code
Congo, The Democratic Republic of the
numeric
   180

2code
Syria
   sy

# Last codes in each set (we'll assume that if we got these, there's a good
# possiblity that we got all the others).

2code
Zimbabwe
alpha-2
   zw

2code
Zimbabwe
alpha-3
   zwe

2code
Zimbabwe
numeric
   716

2code
Zimbabwe
dom
   zw

2code
Zimbabwe
dom
   zw

2code
Zimbabwe
foo
   _undef_

2code
Zipper
dom
retired
   _undef_

##################
# countrycode2code

code2code
bo
alpha-2
alpha-2
   bo

code2code
bo
alpha-3
alpha-3
   _undef_

code2code
zz
alpha-2
alpha-3
   _undef_

code2code
zz
alpha-3
alpha-3
   _undef_

code2code
zz
alpha-2
0
   _undef_

code2code
bo
alpha-2
0
   _undef_

code2code
_blank_
0
0
   _undef_

code2code
BO
alpha-2
alpha-3
   bol

code2code
bol
alpha-3
alpha-2
   bo

code2code
zwe
alpha-3
alpha-2
   zw

code2code
858
numeric
alpha-3
   ury

code2code
858
numeric
alpha-3
   ury

code2code
tr
alpha-2
numeric
   792

code2code
tr
alpha-2
   tr

code2code
   _undef_

###################################
# Test rename_country

2name
gb
   United Kingdom of Great Britain and Northern Ireland

rename
x1
NewName
   0

rename
gb
NewName
foo
   0

rename
gb
Macao
   0

rename
gb
NewName
alpha3
   0

2name
gb
   United Kingdom of Great Britain and Northern Ireland

rename
gb
NewName
   1

2name
gb
   NewName

2name
us
   United States of America

rename
us
The United States
   1

2name
us
   The United States

###################################
# Test add

2name
xx
   _undef_

add
xx
Bolivia
   0

add
fi
Xxxxx
   0

add
xx
Xxxxx
   1

2name
xx
   Xxxxx

add
xx
Xxxxx
foo
   0

add
xy
New Country
alpha-2
   1

add
xyy
New Country
alpha-3
   1

###################################
# Test add_alias

add_alias
FooBar
NewName
   0

add_alias
Australia
Angola
   0

2code
Australia
   au

2code
DownUnder
   _undef_

add_alias
Australia
DownUnder
   1

2code
DownUnder
   au

###################################
# Test delete_alias

2code
uk
   gb

delete_alias
Foobar
   0

delete_alias
UK
   1

2code
uk
   _undef_

delete_alias
Angola
   0

# Complicated example

add
z1
NameA1
alpha-2
   1

add_alias
NameA1
NameA2
alpha-2
   1

add
zz1
NameA2
alpha-3
   1

2name
z1
   NameA1

2name
zz1
alpha-3
   NameA2

code2code
z1
alpha-2
alpha-3
   zz1

delete_alias
NameA2
   1

2name
z1
   NameA1

2name
zz1
alpha-3
   NameA1

# Complicated example

add
z2
NameB1
alpha-2
   1

add_alias
NameB1
NameB2
alpha-2
   1

add
zz2
NameB2
alpha-3
   1

2name
z2
   NameB1

2name
zz2
alpha-3
   NameB2

code2code
z2
alpha-2
alpha-3
   zz2

delete_alias
NameB1
   1

2name
z2
   NameB2

2name
zz2
alpha-3
   NameB2

###################################
# Test delete

2code
Angola
   ao

2code
Angola
alpha-3
   ago

delete
ao
   1

2code
Angola
   _undef_

2code
Angola
alpha-3
   ago

delete
ago
foo
   0

delete
zz
   0

###################################
# Test replace_code

2name
zz
   _undef_

2name
ar
   Argentina

2code
Argentina
   ar

replace_code
ar
us
   0

replace_code
ar
zz
   1

replace_code
us
ar
   0

2name
zz
   Argentina

2name
ar
   Argentina

2code
Argentina
   zz

replace_code
zz
ar
   1

2name
zz
   Argentina

2name
ar
   Argentina

2code
Argentina
   ar

replace_code
ar
z2
foo
   0

replace_code
ar
z2
alpha-3
   0

###################################
# Test add_code_alias and
# delete_code_alias

2name
bm
   Bermuda

2name
yy
   _undef_

2code
Bermuda
   bm

add_code_alias
bm
us
   0

add_code_alias
bm
zz
   0

add_code_alias
bm
yy
   1

2name
bm
   Bermuda

2name
yy
   Bermuda

2code
Bermuda
   bm

delete_code_alias
us
   0

delete_code_alias
ww
   0

delete_code_alias
yy
   1

2name
bm
   Bermuda

2name
yy
   _undef_

2code
Bermuda
   bm

add_code_alias
bm
yy
   1

2name
yy
   Bermuda

add
yy
Foo
   0

delete
bm
   1

2name
bm
   _undef_

add_code_alias
bm
y2
foo
   0

add_code_alias
bm
y2
alpha-3
   0

delete_code_alias
bm
foo
   0
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

