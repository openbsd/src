#!/usr/bin/perl

# Testing documents that should fail

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use File::Spec::Functions ':ALL';
use t::lib::Test;
use Test::More tests => 20;
use CPAN::Meta::YAML ();

my $FEATURE = 'does not support a feature';
my $PLAIN   = 'illegal characters in plain scalar';





#####################################################################
# Syntactic Errors

yaml_error( <<'END_YAML', $FEATURE );
- 'Multiline
quote'
END_YAML

yaml_error( <<'END_YAML', $FEATURE );
- "Multiline
quote"
END_YAML

yaml_error( <<'END_YAML', $FEATURE );
---
version: !!perl/hash:version 
  original: v2.0.2
  qv: 1
  version: 
    - 2
    - 0
    - 2
END_YAML

yaml_error( <<'END_YAML', $PLAIN );
- - 2
END_YAML

yaml_error( <<'END_YAML', $PLAIN );
foo: -
END_YAML

yaml_error( <<'END_YAML', $PLAIN );
foo: @INC
END_YAML

yaml_error( <<'END_YAML', $PLAIN );
foo: %INC
END_YAML

yaml_error( <<'END_YAML', $PLAIN );
foo: bar:
END_YAML

yaml_error( <<'END_YAML', $PLAIN );
foo: bar: baz
END_YAML

yaml_error( <<'END_YAML', $PLAIN );
foo: `perl -V`
END_YAML
