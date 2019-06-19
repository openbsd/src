#!/usr/bin/perl
# Copyright (c) 2016-2018 Sullivan Beck. All rights reserved.
# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.

use warnings;
use strict;

$::tests = '';

$::tests = "

all_names
2
   ~
   Abkhazian
   Afar

all_codes
2
   ~
   aa
   ab

2name
zu
   Zulu

rename
zu
NewName
foo
   0

rename
zu
English
alpha-2
   0

rename
zu
NewName
alpha-3
   0

2name
zu
   Zulu

rename
zu
NewName
alpha-2
   1

2name
zu
   NewName

2code
Afar
   aa

2code
ESTONIAN
   et

2code
French
   fr

2code
Greek
   el

2code
Japanese
   ja

2code
Zulu
   zu

2code
english
   en

2code
japanese
   ja

# Last ones in the list

2code
Zulu
alpha-2
   zu

2code
Zaza
alpha-3
   zza

2code
Welsh
term
   cym

2code
Zande languages
alpha-3
   znd

2code
Zuojiang Zhuang
alpha-3
   zzj

2name
in
   _undef_

2name
iw
   _undef_

2name
ji
   _undef_

2name
jp
   _undef_

2name
zz
   _undef_

2name
DA
   Danish

2name
aa
   Afar

2name
ae
   Avestan

2name
bs
   Bosnian

2name
ce
   Chechen

2name
ch
   Chamorro

2name
cu
   Church Slavic

2name
cv
   Chuvash

2name
en
   English

2name
eo
   Esperanto

2name
fi
   Finnish

2name
gv
   Manx

2name
he
   Hebrew

2name
ho
   Hiri Motu

2name
hz
   Herero

2name
id
   Indonesian

2name
iu
   Inuktitut

2name
ki
   Kikuyu

2name
kj
   Kuanyama

2name
kv
   Komi

2name
kw
   Cornish

2name
lb
   Luxembourgish

2name
mh
   Marshallese

2name
nb
   Norwegian Bokmal

2name
nd
   North Ndebele

2name
ng
   Ndonga

2name
nn
   Norwegian Nynorsk

2name
nr
   South Ndebele

2name
nv
   Navajo

2name
ny
   Nyanja

2name
oc
   Occitan (post 1500)

2name
os
   Ossetian

2name
pi
   Pali

2name
sc
   Sardinian

2name
se
   Northern Sami

2name
ug
   Uighur

2name
yi
   Yiddish

2name
za
   Zhuang

code2code
zu
alpha-2
alpha-3
   zul

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

