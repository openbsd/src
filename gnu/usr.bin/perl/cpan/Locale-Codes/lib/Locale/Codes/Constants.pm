package Locale::Codes::Constants;
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
our(@CONSTANTS,%ALL_CODESETS);

our(@CONSTANTS_COUNTRY) = qw(
                LOCALE_CODE_ALPHA_2
                LOCALE_CODE_ALPHA_3
                LOCALE_CODE_DOM
                LOCALE_CODE_GENC_ALPHA_2
                LOCALE_CODE_GENC_ALPHA_3
                LOCALE_CODE_GENC_NUMERIC
                LOCALE_CODE_NUMERIC
                LOCALE_CODE_UN_ALPHA_3
                LOCALE_CODE_UN_NUMERIC
                LOCALE_COUNTRY_ALPHA_2
                LOCALE_COUNTRY_ALPHA_3
                LOCALE_COUNTRY_DOM
                LOCALE_COUNTRY_GENC_ALPHA_2
                LOCALE_COUNTRY_GENC_ALPHA_3
                LOCALE_COUNTRY_GENC_NUMERIC
                LOCALE_COUNTRY_NUMERIC
                LOCALE_COUNTRY_UN_ALPHA_3
                LOCALE_COUNTRY_UN_NUMERIC
);
push(@CONSTANTS,@CONSTANTS_COUNTRY);

our(@CONSTANTS_CURRENCY) = qw(
                LOCALE_CURRENCY_ALPHA
                LOCALE_CURRENCY_NUMERIC
                LOCALE_CURR_ALPHA
                LOCALE_CURR_NUMERIC
);
push(@CONSTANTS,@CONSTANTS_CURRENCY);

our(@CONSTANTS_LANGEXT) = qw(
                LOCALE_LANGEXT_ALPHA
);
push(@CONSTANTS,@CONSTANTS_LANGEXT);

our(@CONSTANTS_LANGFAM) = qw(
                LOCALE_LANGFAM_ALPHA
);
push(@CONSTANTS,@CONSTANTS_LANGFAM);

our(@CONSTANTS_LANGUAGE) = qw(
                LOCALE_LANGUAGE_ALPHA_2
                LOCALE_LANGUAGE_ALPHA_3
                LOCALE_LANGUAGE_TERM
                LOCALE_LANG_ALPHA_2
                LOCALE_LANG_ALPHA_3
                LOCALE_LANG_TERM
);
push(@CONSTANTS,@CONSTANTS_LANGUAGE);

our(@CONSTANTS_LANGVAR) = qw(
                LOCALE_LANGVAR_ALPHA
);
push(@CONSTANTS,@CONSTANTS_LANGVAR);

our(@CONSTANTS_SCRIPT) = qw(
                LOCALE_SCRIPT_ALPHA
                LOCALE_SCRIPT_NUMERIC
);
push(@CONSTANTS,@CONSTANTS_SCRIPT);

@EXPORT    = (@CONSTANTS,
               qw(
                %ALL_CODESETS
               ));

use constant LOCALE_CODE_ALPHA_2         => 'alpha-2';
use constant LOCALE_CODE_ALPHA_3         => 'alpha-3';
use constant LOCALE_CODE_DOM             => 'dom';
use constant LOCALE_CODE_GENC_ALPHA_2    => 'genc-alpha-2';
use constant LOCALE_CODE_GENC_ALPHA_3    => 'genc-alpha-3';
use constant LOCALE_CODE_GENC_NUMERIC    => 'genc-numeric';
use constant LOCALE_CODE_NUMERIC         => 'numeric';
use constant LOCALE_CODE_UN_ALPHA_3      => 'un-alpha-3';
use constant LOCALE_CODE_UN_NUMERIC      => 'un-numeric';
use constant LOCALE_COUNTRY_ALPHA_2      => 'alpha-2';
use constant LOCALE_COUNTRY_ALPHA_3      => 'alpha-3';
use constant LOCALE_COUNTRY_DOM          => 'dom';
use constant LOCALE_COUNTRY_GENC_ALPHA_2 => 'genc-alpha-2';
use constant LOCALE_COUNTRY_GENC_ALPHA_3 => 'genc-alpha-3';
use constant LOCALE_COUNTRY_GENC_NUMERIC => 'genc-numeric';
use constant LOCALE_COUNTRY_NUMERIC      => 'numeric';
use constant LOCALE_COUNTRY_UN_ALPHA_3   => 'un-alpha-3';
use constant LOCALE_COUNTRY_UN_NUMERIC   => 'un-numeric';

$ALL_CODESETS{'country'} =
   {
      'default'  => 'alpha-2',
      'module'   => 'Country',
      'codesets' => {
                     'alpha-2'      => ['lc'],
                     'alpha-3'      => ['lc'],
                     'dom'          => ['lc'],
                     'genc-alpha-2' => ['uc'],
                     'genc-alpha-3' => ['uc'],
                     'genc-numeric' => ['numeric',3],
                     'numeric'      => ['numeric',3],
                     'un-alpha-3'   => ['uc'],
                     'un-numeric'   => ['numeric',3],
                    }
   };

use constant LOCALE_CURRENCY_ALPHA       => 'alpha';
use constant LOCALE_CURRENCY_NUMERIC     => 'num';
use constant LOCALE_CURR_ALPHA           => 'alpha';
use constant LOCALE_CURR_NUMERIC         => 'num';

$ALL_CODESETS{'currency'} =
   {
      'default'  => 'alpha',
      'module'   => 'Currency',
      'codesets' => {
                     'alpha'        => ['uc'],
                     'num'          => ['numeric',3],
                    }
   };

use constant LOCALE_LANGEXT_ALPHA        => 'alpha';

$ALL_CODESETS{'langext'} =
   {
      'default'  => 'alpha',
      'module'   => 'LangExt',
      'codesets' => {
                     'alpha'        => ['lc'],
                    }
   };

use constant LOCALE_LANGFAM_ALPHA        => 'alpha';

$ALL_CODESETS{'langfam'} =
   {
      'default'  => 'alpha',
      'module'   => 'LangFam',
      'codesets' => {
                     'alpha'        => ['lc'],
                    }
   };

use constant LOCALE_LANGUAGE_ALPHA_2     => 'alpha-2';
use constant LOCALE_LANGUAGE_ALPHA_3     => 'alpha-3';
use constant LOCALE_LANGUAGE_TERM        => 'term';
use constant LOCALE_LANG_ALPHA_2         => 'alpha-2';
use constant LOCALE_LANG_ALPHA_3         => 'alpha-3';
use constant LOCALE_LANG_TERM            => 'term';

$ALL_CODESETS{'language'} =
   {
      'default'  => 'alpha-2',
      'module'   => 'Language',
      'codesets' => {
                     'alpha-2'      => ['lc'],
                     'alpha-3'      => ['lc'],
                     'term'         => ['lc'],
                    }
   };

use constant LOCALE_LANGVAR_ALPHA        => 'alpha';

$ALL_CODESETS{'langvar'} =
   {
      'default'  => 'alpha',
      'module'   => 'LangVar',
      'codesets' => {
                     'alpha'        => ['lc'],
                    }
   };

use constant LOCALE_SCRIPT_ALPHA         => 'alpha';
use constant LOCALE_SCRIPT_NUMERIC       => 'num';

$ALL_CODESETS{'script'} =
   {
      'default'  => 'alpha',
      'module'   => 'Script',
      'codesets' => {
                     'alpha'        => ['ucfirst'],
                     'num'          => ['numeric',3],
                    }
   };


1;
