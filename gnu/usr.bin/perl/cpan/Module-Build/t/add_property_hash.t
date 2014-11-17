#!/usr/bin/perl -w

use strict;
use lib 't/lib';
use MBTest tests => 1;

blib_load 'Module::Build';

ADDPROP: {
  package My::Build::Prop;
  use base 'Module::Build';
  __PACKAGE__->add_property( 'hash_property' => {});
}

ok grep { $_ eq 'install_path' } My::Build::Prop->hash_properties, "has install_path even after adding another hash property";

