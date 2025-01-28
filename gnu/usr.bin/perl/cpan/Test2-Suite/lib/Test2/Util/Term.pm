package Test2::Util::Term;
use strict;
use warnings;

use Term::Table::Util qw/term_size USE_GCS USE_TERM_READKEY uni_length/;

our $VERSION = '0.000162';

use Test2::Util::Importer 'Test2::Util::Importer' => 'import';
our @EXPORT_OK = qw/term_size USE_GCS USE_TERM_READKEY uni_length/;

1;
