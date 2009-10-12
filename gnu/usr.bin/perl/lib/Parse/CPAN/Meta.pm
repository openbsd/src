package Parse::CPAN::Meta;

use strict;
use Carp 'croak';

# UTF Support?
sub HAVE_UTF8 () { $] >= 5.007003 }
BEGIN {
	if ( HAVE_UTF8 ) {
		# The string eval helps hide this from Test::MinimumVersion
		eval "require utf8;";
		die "Failed to load UTF-8 support" if $@;
	}

	# Class structure
	require 5.004;
	require Exporter;
	$Parse::CPAN::Meta::VERSION   = '1.39';
	@Parse::CPAN::Meta::ISA       = qw{ Exporter      };
	@Parse::CPAN::Meta::EXPORT_OK = qw{ Load LoadFile };
}

# Prototypes
sub LoadFile ($);
sub Load     ($);
sub _scalar  ($$$);
sub _array   ($$$);
sub _hash    ($$$);

# Printable characters for escapes
my %UNESCAPES = (
	z => "\x00", a => "\x07", t    => "\x09",
	n => "\x0a", v => "\x0b", f    => "\x0c",
	r => "\x0d", e => "\x1b", '\\' => '\\',
);





#####################################################################
# Implementation

# Create an object from a file
sub LoadFile ($) {
	# Check the file
	my $file = shift;
	croak('You did not specify a file name')            unless $file;
	croak( "File '$file' does not exist" )              unless -e $file;
	croak( "'$file' is a directory, not a file" )       unless -f _;
	croak( "Insufficient permissions to read '$file'" ) unless -r _;

	# Slurp in the file
	local $/ = undef;
	local *CFG;
	unless ( open( CFG, $file ) ) {
		croak("Failed to open file '$file': $!");
	}
	my $yaml = <CFG>;
	unless ( close(CFG) ) {
		croak("Failed to close file '$file': $!");
	}

	# Hand off to the actual parser
	Load( $yaml );
}

# Parse a document from a string.
# Doing checks on $_[0] prevents us having to do a string copy.
sub Load ($) {
	my $string = $_[0];
	unless ( defined $string ) {
		croak("Did not provide a string to load");
	}

	# Byte order marks
	if ( $string =~ /^(?:\376\377|\377\376|\377\376\0\0|\0\0\376\377)/ ) {
		croak("Stream has a non UTF-8 Unicode Byte Order Mark");
	} else {
		# Strip UTF-8 bom if found, we'll just ignore it
		$string =~ s/^\357\273\277//;
	}

	# Try to decode as utf8
	utf8::decode($string) if HAVE_UTF8;

	# Check for some special cases
	return () unless length $string;
	unless ( $string =~ /[\012\015]+\z/ ) {
		croak("Stream does not end with newline character");
	}

	# Split the file into lines
	my @lines = grep { ! /^\s*(?:\#.*)?\z/ }
	            split /(?:\015{1,2}\012|\015|\012)/, $string;

	# Strip the initial YAML header
	@lines and $lines[0] =~ /^\%YAML[: ][\d\.]+.*\z/ and shift @lines;

	# A nibbling parser
	my @documents = ();
	while ( @lines ) {
		# Do we have a document header?
		if ( $lines[0] =~ /^---\s*(?:(.+)\s*)?\z/ ) {
			# Handle scalar documents
			shift @lines;
			if ( defined $1 and $1 !~ /^(?:\#.+|\%YAML[: ][\d\.]+)\z/ ) {
				push @documents, _scalar( "$1", [ undef ], \@lines );
				next;
			}
		}

		if ( ! @lines or $lines[0] =~ /^(?:---|\.\.\.)/ ) {
			# A naked document
			push @documents, undef;
			while ( @lines and $lines[0] !~ /^---/ ) {
				shift @lines;
			}

		} elsif ( $lines[0] =~ /^\s*\-/ ) {
			# An array at the root
			my $document = [ ];
			push @documents, $document;
			_array( $document, [ 0 ], \@lines );

		} elsif ( $lines[0] =~ /^(\s*)\S/ ) {
			# A hash at the root
			my $document = { };
			push @documents, $document;
			_hash( $document, [ length($1) ], \@lines );

		} else {
			croak("Parse::CPAN::Meta failed to classify line '$lines[0]'");
		}
	}

	if ( wantarray ) {
		return @documents;
	} else {
		return $documents[-1];
	}
}

# Deparse a scalar string to the actual scalar
sub _scalar ($$$) {
	my ($string, $indent, $lines) = @_;

	# Trim trailing whitespace
	$string =~ s/\s*\z//;

	# Explitic null/undef
	return undef if $string eq '~';

	# Quotes
	if ( $string =~ /^\'(.*?)\'\z/ ) {
		return '' unless defined $1;
		$string = $1;
		$string =~ s/\'\'/\'/g;
		return $string;
	}
	if ( $string =~ /^\"((?:\\.|[^\"])*)\"\z/ ) {
		# Reusing the variable is a little ugly,
		# but avoids a new variable and a string copy.
		$string = $1;
		$string =~ s/\\"/"/g;
		$string =~ s/\\([never\\fartz]|x([0-9a-fA-F]{2}))/(length($1)>1)?pack("H2",$2):$UNESCAPES{$1}/gex;
		return $string;
	}

	# Special cases
	if ( $string =~ /^[\'\"!&]/ ) {
		croak("Parse::CPAN::Meta does not support a feature in line '$lines->[0]'");
	}
	return {} if $string eq '{}';
	return [] if $string eq '[]';

	# Regular unquoted string
	return $string unless $string =~ /^[>|]/;

	# Error
	croak("Parse::CPAN::Meta failed to find multi-line scalar content") unless @$lines;

	# Check the indent depth
	$lines->[0]   =~ /^(\s*)/;
	$indent->[-1] = length("$1");
	if ( defined $indent->[-2] and $indent->[-1] <= $indent->[-2] ) {
		croak("Parse::CPAN::Meta found bad indenting in line '$lines->[0]'");
	}

	# Pull the lines
	my @multiline = ();
	while ( @$lines ) {
		$lines->[0] =~ /^(\s*)/;
		last unless length($1) >= $indent->[-1];
		push @multiline, substr(shift(@$lines), length($1));
	}

	my $j = (substr($string, 0, 1) eq '>') ? ' ' : "\n";
	my $t = (substr($string, 1, 1) eq '-') ? ''  : "\n";
	return join( $j, @multiline ) . $t;
}

# Parse an array
sub _array ($$$) {
	my ($array, $indent, $lines) = @_;

	while ( @$lines ) {
		# Check for a new document
		if ( $lines->[0] =~ /^(?:---|\.\.\.)/ ) {
			while ( @$lines and $lines->[0] !~ /^---/ ) {
				shift @$lines;
			}
			return 1;
		}

		# Check the indent level
		$lines->[0] =~ /^(\s*)/;
		if ( length($1) < $indent->[-1] ) {
			return 1;
		} elsif ( length($1) > $indent->[-1] ) {
			croak("Parse::CPAN::Meta found bad indenting in line '$lines->[0]'");
		}

		if ( $lines->[0] =~ /^(\s*\-\s+)[^\'\"]\S*\s*:(?:\s+|$)/ ) {
			# Inline nested hash
			my $indent2 = length("$1");
			$lines->[0] =~ s/-/ /;
			push @$array, { };
			_hash( $array->[-1], [ @$indent, $indent2 ], $lines );

		} elsif ( $lines->[0] =~ /^\s*\-(\s*)(.+?)\s*\z/ ) {
			# Array entry with a value
			shift @$lines;
			push @$array, _scalar( "$2", [ @$indent, undef ], $lines );

		} elsif ( $lines->[0] =~ /^\s*\-\s*\z/ ) {
			shift @$lines;
			unless ( @$lines ) {
				push @$array, undef;
				return 1;
			}
			if ( $lines->[0] =~ /^(\s*)\-/ ) {
				my $indent2 = length("$1");
				if ( $indent->[-1] == $indent2 ) {
					# Null array entry
					push @$array, undef;
				} else {
					# Naked indenter
					push @$array, [ ];
					_array( $array->[-1], [ @$indent, $indent2 ], $lines );
				}

			} elsif ( $lines->[0] =~ /^(\s*)\S/ ) {
				push @$array, { };
				_hash( $array->[-1], [ @$indent, length("$1") ], $lines );

			} else {
				croak("Parse::CPAN::Meta failed to classify line '$lines->[0]'");
			}

		} elsif ( defined $indent->[-2] and $indent->[-1] == $indent->[-2] ) {
			# This is probably a structure like the following...
			# ---
			# foo:
			# - list
			# bar: value
			#
			# ... so lets return and let the hash parser handle it
			return 1;

		} else {
			croak("Parse::CPAN::Meta failed to classify line '$lines->[0]'");
		}
	}

	return 1;
}

# Parse an array
sub _hash ($$$) {
	my ($hash, $indent, $lines) = @_;

	while ( @$lines ) {
		# Check for a new document
		if ( $lines->[0] =~ /^(?:---|\.\.\.)/ ) {
			while ( @$lines and $lines->[0] !~ /^---/ ) {
				shift @$lines;
			}
			return 1;
		}

		# Check the indent level
		$lines->[0] =~ /^(\s*)/;
		if ( length($1) < $indent->[-1] ) {
			return 1;
		} elsif ( length($1) > $indent->[-1] ) {
			croak("Parse::CPAN::Meta found bad indenting in line '$lines->[0]'");
		}

		# Get the key
		unless ( $lines->[0] =~ s/^\s*([^\'\" ][^\n]*?)\s*:(\s+|$)// ) {
			if ( $lines->[0] =~ /^\s*[?\'\"]/ ) {
				croak("Parse::CPAN::Meta does not support a feature in line '$lines->[0]'");
			}
			croak("Parse::CPAN::Meta failed to classify line '$lines->[0]'");
		}
		my $key = $1;

		# Do we have a value?
		if ( length $lines->[0] ) {
			# Yes
			$hash->{$key} = _scalar( shift(@$lines), [ @$indent, undef ], $lines );
		} else {
			# An indent
			shift @$lines;
			unless ( @$lines ) {
				$hash->{$key} = undef;
				return 1;
			}
			if ( $lines->[0] =~ /^(\s*)-/ ) {
				$hash->{$key} = [];
				_array( $hash->{$key}, [ @$indent, length($1) ], $lines );
			} elsif ( $lines->[0] =~ /^(\s*)./ ) {
				my $indent2 = length("$1");
				if ( $indent->[-1] >= $indent2 ) {
					# Null hash entry
					$hash->{$key} = undef;
				} else {
					$hash->{$key} = {};
					_hash( $hash->{$key}, [ @$indent, length($1) ], $lines );
				}
			}
		}
	}

	return 1;
}

1;

__END__

=pod

=head1 NAME

Parse::CPAN::Meta - Parse META.yml and other similar CPAN metadata files

=head1 SYNOPSIS

    #############################################
    # In your file
    
    ---
    rootproperty: blah
    section:
      one: two
      three: four
      Foo: Bar
      empty: ~
    
    
    
    #############################################
    # In your program
    
    use Parse::CPAN::Meta;
    
    # Create a YAML file
    my @yaml = Parse::CPAN::Meta::LoadFile( 'Meta.yml' );
    
    # Reading properties
    my $root = $yaml[0]->{rootproperty};
    my $one  = $yaml[0]->{section}->{one};
    my $Foo  = $yaml[0]->{section}->{Foo};

=head1 DESCRIPTION

B<Parse::CPAN::Meta> is a parser for F<META.yml> files, based on the
parser half of L<YAML::Tiny>.

It supports a basic subset of the full YAML specification, enough to
implement parsing of typical F<META.yml> files, and other similarly simple
YAML files.

If you need something with more power, move up to a full YAML parser such
as L<YAML>, L<YAML::Syck> or L<YAML::LibYAML>.

B<Parse::CPAN::Meta> provides a very simply API of only two functions,
based on the YAML functions of the same name. Wherever possible,
identical calling semantics are used.

All error reporting is done with exceptions (die'ing).

=head1 FUNCTIONS

For maintenance clarity, no functions are exported.

=head2 Load

  my @yaml = Load( $string );

Parses a string containing a valid YAML stream into a list of Perl data
structures.

=head2 LoadFile

  my @yaml = LoadFile( 'META.yml' );

Reads the YAML stream from a file instead of a string.

=head1 SUPPORT

Bugs should be reported via the CPAN bug tracker at

L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=Parse-CPAN-Meta>

=head1 AUTHOR

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

=head1 SEE ALSO

L<YAML>, L<YAML::Syck>, L<Config::Tiny>, L<CSS::Tiny>,
L<http://use.perl.org/~Alias/journal/29427>, L<http://ali.as/>

=head1 COPYRIGHT

Copyright 2006 - 2009 Adam Kennedy.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
