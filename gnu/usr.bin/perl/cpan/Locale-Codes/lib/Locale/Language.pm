package Locale::Language;
# Copyright (C) 2001      Canon Research Centre Europe (CRE).
# Copyright (C) 2002-2009 Neil Bowers
# Copyright (c) 2010-2018 Sullivan Beck
# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.

# This file was automatically generated.  Any changes to this file will
# be lost the next time 'gen_mods' is run.
#    Generated on: Fri Feb 23 12:55:25 EST 2018

use strict;
use warnings;
require 5.006;
use Exporter qw(import);

our($VERSION,@EXPORT);
$VERSION   = '3.56';

################################################################################
use if $] >= 5.027007, 'deprecate';
use Locale::Codes;
use Locale::Codes::Constants;

@EXPORT    = qw(
                code2language
                language2code
                all_language_codes
                all_language_names
                language_code2code
               );
push(@EXPORT,@Locale::Codes::Constants::CONSTANTS_LANGUAGE);

our $obj = new Locale::Codes('language');
$obj->show_errors(0);

sub show_errors {
   my($val) = @_;
   $obj->show_errors($val);
}

sub code2language {
   return $obj->code2name(@_);
}

sub language2code {
   return $obj->name2code(@_);
}

sub language_code2code {
   return $obj->code2code(@_);
}

sub all_language_codes {
   return $obj->all_codes(@_);
}

sub all_language_names {
   return $obj->all_names(@_);
}

sub rename_language {
   return $obj->rename_code(@_);
}

sub add_language {
   return $obj->add_code(@_);
}

sub delete_language {
   return $obj->delete_code(@_);
}

sub add_language_alias {
   return $obj->add_alias(@_);
}

sub delete_language_alias {
   return $obj->delete_alias(@_);
}

sub rename_language_code {
   return $obj->replace_code(@_);
}

sub add_language_code_alias {
   return $obj->add_code_alias(@_);
}

sub delete_language_code_alias {
   return $obj->delete_code_alias(@_);
}

1;
