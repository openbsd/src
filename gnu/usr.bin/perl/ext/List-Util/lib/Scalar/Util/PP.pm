# Scalar::Util::PP.pm
#
# Copyright (c) 1997-2009 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.
#
# This module is normally only loaded if the XS module is not available

package Scalar::Util::PP;

use strict;
use warnings;
use vars qw(@ISA @EXPORT $VERSION $recurse);
require Exporter;
use B qw(svref_2object);

@ISA     = qw(Exporter);
@EXPORT  = qw(blessed reftype tainted readonly refaddr looks_like_number);
$VERSION = "1.21";
$VERSION = eval $VERSION;

sub blessed ($) {
  return undef unless length(ref($_[0]));
  my $b = svref_2object($_[0]);
  return undef unless $b->isa('B::PVMG');
  my $s = $b->SvSTASH;
  return $s->isa('B::HV') ? $s->NAME : undef;
}

sub refaddr($) {
  return undef unless length(ref($_[0]));

  my $addr;
  if(defined(my $pkg = blessed($_[0]))) {
    $addr .= bless $_[0], 'Scalar::Util::Fake';
    bless $_[0], $pkg;
  }
  else {
    $addr .= $_[0]
  }

  $addr =~ /0x(\w+)/;
  local $^W;
  hex($1);
}

{
  my %tmap = qw(
    B::HV HASH
    B::AV ARRAY
    B::CV CODE
    B::IO IO
    B::NULL SCALAR
    B::NV SCALAR
    B::PV SCALAR
    B::GV GLOB
    B::RV REF
    B::REGEXP REGEXP
  );

  sub reftype ($) {
    my $r = shift;

    return undef unless length(ref($r));

    my $t = ref(svref_2object($r));

    return
        exists $tmap{$t} ? $tmap{$t}
      : length(ref($$r)) ? 'REF'
      :                    'SCALAR';
  }
}

sub tainted {
  local($@, $SIG{__DIE__}, $SIG{__WARN__});
  local $^W = 0;
  no warnings;
  eval { kill 0 * $_[0] };
  $@ =~ /^Insecure/;
}

sub readonly {
  return 0 if tied($_[0]) || (ref(\($_[0])) ne "SCALAR");

  local($@, $SIG{__DIE__}, $SIG{__WARN__});
  my $tmp = $_[0];

  !eval { $_[0] = $tmp; 1 };
}

sub looks_like_number {
  local $_ = shift;

  # checks from perlfaq4
  return 0 if !defined($_);
  if (ref($_)) {
    require overload;
    return overload::Overloaded($_) ? defined(0 + $_) : 0;
  }
  return 1 if (/^[+-]?\d+$/); # is a +/- integer
  return 1 if (/^([+-]?)(?=\d|\.\d)\d*(\.\d*)?([Ee]([+-]?\d+))?$/); # a C float
  return 1 if ($] >= 5.008 and /^(Inf(inity)?|NaN)$/i) or ($] >= 5.006001 and /^Inf$/i);

  0;
}


1;
