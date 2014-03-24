package CPAN::Meta::YAML;
{
  $CPAN::Meta::YAML::VERSION = '0.008';
}

use strict;

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
	require Carp;
	@CPAN::Meta::YAML::ISA       = qw{ Exporter  };
	@CPAN::Meta::YAML::EXPORT    = qw{ Load Dump };
	@CPAN::Meta::YAML::EXPORT_OK = qw{ LoadFile DumpFile freeze thaw };

	# Error storage
	$CPAN::Meta::YAML::errstr    = '';
}

# The character class of all characters we need to escape
# NOTE: Inlined, since it's only used once
# my $RE_ESCAPE = '[\\x00-\\x08\\x0b-\\x0d\\x0e-\\x1f\"\n]';

# Printed form of the unprintable characters in the lowest range
# of ASCII characters, listed by ASCII ordinal position.
my @UNPRINTABLE = qw(
	z    x01  x02  x03  x04  x05  x06  a
	x08  t    n    v    f    r    x0e  x0f
	x10  x11  x12  x13  x14  x15  x16  x17
	x18  x19  x1a  e    x1c  x1d  x1e  x1f
);

# Printable characters for escapes
my %UNESCAPES = (
	z => "\x00", a => "\x07", t    => "\x09",
	n => "\x0a", v => "\x0b", f    => "\x0c",
	r => "\x0d", e => "\x1b", '\\' => '\\',
);

# Special magic boolean words
my %QUOTE = map { $_ => 1 } qw{
	null Null NULL
	y Y yes Yes YES n N no No NO
	true True TRUE false False FALSE
	on On ON off Off OFF
};





#####################################################################
# Implementation

# Create an empty CPAN::Meta::YAML object
sub new {
	my $class = shift;
	bless [ @_ ], $class;
}

# Create an object from a file
sub read {
	my $class = ref $_[0] ? ref shift : shift;

	# Check the file
	my $file = shift or return $class->_error( 'You did not specify a file name' );
	return $class->_error( "File '$file' does not exist" )              unless -e $file;
	return $class->_error( "'$file' is a directory, not a file" )       unless -f _;
	return $class->_error( "Insufficient permissions to read '$file'" ) unless -r _;

	# Slurp in the file
	local $/ = undef;
	local *CFG;
	unless ( open(CFG, $file) ) {
		return $class->_error("Failed to open file '$file': $!");
	}
	my $contents = <CFG>;
	unless ( close(CFG) ) {
		return $class->_error("Failed to close file '$file': $!");
	}

	$class->read_string( $contents );
}

# Create an object from a string
sub read_string {
	my $class  = ref $_[0] ? ref shift : shift;
	my $self   = bless [], $class;
	my $string = $_[0];
	eval {
		unless ( defined $string ) {
			die \"Did not provide a string to load";
		}

		# Byte order marks
		# NOTE: Keeping this here to educate maintainers
		# my %BOM = (
		#     "\357\273\277" => 'UTF-8',
		#     "\376\377"     => 'UTF-16BE',
		#     "\377\376"     => 'UTF-16LE',
		#     "\377\376\0\0" => 'UTF-32LE'
		#     "\0\0\376\377" => 'UTF-32BE',
		# );
		if ( $string =~ /^(?:\376\377|\377\376|\377\376\0\0|\0\0\376\377)/ ) {
			die \"Stream has a non UTF-8 BOM";
		} else {
			# Strip UTF-8 bom if found, we'll just ignore it
			$string =~ s/^\357\273\277//;
		}

		# Try to decode as utf8
		utf8::decode($string) if HAVE_UTF8;

		# Check for some special cases
		return $self unless length $string;
		unless ( $string =~ /[\012\015]+\z/ ) {
			die \"Stream does not end with newline character";
		}

		# Split the file into lines
		my @lines = grep { ! /^\s*(?:\#.*)?\z/ }
			    split /(?:\015{1,2}\012|\015|\012)/, $string;

		# Strip the initial YAML header
		@lines and $lines[0] =~ /^\%YAML[: ][\d\.]+.*\z/ and shift @lines;

		# A nibbling parser
		while ( @lines ) {
			# Do we have a document header?
			if ( $lines[0] =~ /^---\s*(?:(.+)\s*)?\z/ ) {
				# Handle scalar documents
				shift @lines;
				if ( defined $1 and $1 !~ /^(?:\#.+|\%YAML[: ][\d\.]+)\z/ ) {
					push @$self, $self->_read_scalar( "$1", [ undef ], \@lines );
					next;
				}
			}

			if ( ! @lines or $lines[0] =~ /^(?:---|\.\.\.)/ ) {
				# A naked document
				push @$self, undef;
				while ( @lines and $lines[0] !~ /^---/ ) {
					shift @lines;
				}

			} elsif ( $lines[0] =~ /^\s*\-/ ) {
				# An array at the root
				my $document = [ ];
				push @$self, $document;
				$self->_read_array( $document, [ 0 ], \@lines );

			} elsif ( $lines[0] =~ /^(\s*)\S/ ) {
				# A hash at the root
				my $document = { };
				push @$self, $document;
				$self->_read_hash( $document, [ length($1) ], \@lines );

			} else {
				die \"CPAN::Meta::YAML failed to classify the line '$lines[0]'";
			}
		}
	};
	if ( ref $@ eq 'SCALAR' ) {
		return $self->_error(${$@});
	} elsif ( $@ ) {
		require Carp;
		Carp::croak($@);
	}

	return $self;
}

# Deparse a scalar string to the actual scalar
sub _read_scalar {
	my ($self, $string, $indent, $lines) = @_;

	# Trim trailing whitespace
	$string =~ s/\s*\z//;

	# Explitic null/undef
	return undef if $string eq '~';

	# Single quote
	if ( $string =~ /^\'(.*?)\'(?:\s+\#.*)?\z/ ) {
		return '' unless defined $1;
		$string = $1;
		$string =~ s/\'\'/\'/g;
		return $string;
	}

	# Double quote.
	# The commented out form is simpler, but overloaded the Perl regex
	# engine due to recursion and backtracking problems on strings
	# larger than 32,000ish characters. Keep it for reference purposes.
	# if ( $string =~ /^\"((?:\\.|[^\"])*)\"\z/ ) {
	if ( $string =~ /^\"([^\\"]*(?:\\.[^\\"]*)*)\"(?:\s+\#.*)?\z/ ) {
		# Reusing the variable is a little ugly,
		# but avoids a new variable and a string copy.
		$string = $1;
		$string =~ s/\\"/"/g;
		$string =~ s/\\([never\\fartz]|x([0-9a-fA-F]{2}))/(length($1)>1)?pack("H2",$2):$UNESCAPES{$1}/gex;
		return $string;
	}

	# Special cases
	if ( $string =~ /^[\'\"!&]/ ) {
		die \"CPAN::Meta::YAML does not support a feature in line '$string'";
	}
	return {} if $string =~ /^{}(?:\s+\#.*)?\z/;
	return [] if $string =~ /^\[\](?:\s+\#.*)?\z/;

	# Regular unquoted string
	if ( $string !~ /^[>|]/ ) {
		if (
			$string =~ /^(?:-(?:\s|$)|[\@\%\`])/
			or
			$string =~ /:(?:\s|$)/
		) {
			die \"CPAN::Meta::YAML found illegal characters in plain scalar: '$string'";
		}
		$string =~ s/\s+#.*\z//;
		return $string;
	}

	# Error
	die \"CPAN::Meta::YAML failed to find multi-line scalar content" unless @$lines;

	# Check the indent depth
	$lines->[0]   =~ /^(\s*)/;
	$indent->[-1] = length("$1");
	if ( defined $indent->[-2] and $indent->[-1] <= $indent->[-2] ) {
		die \"CPAN::Meta::YAML found bad indenting in line '$lines->[0]'";
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
sub _read_array {
	my ($self, $array, $indent, $lines) = @_;

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
			die \"CPAN::Meta::YAML found bad indenting in line '$lines->[0]'";
		}

		if ( $lines->[0] =~ /^(\s*\-\s+)[^\'\"]\S*\s*:(?:\s+|$)/ ) {
			# Inline nested hash
			my $indent2 = length("$1");
			$lines->[0] =~ s/-/ /;
			push @$array, { };
			$self->_read_hash( $array->[-1], [ @$indent, $indent2 ], $lines );

		} elsif ( $lines->[0] =~ /^\s*\-(\s*)(.+?)\s*\z/ ) {
			# Array entry with a value
			shift @$lines;
			push @$array, $self->_read_scalar( "$2", [ @$indent, undef ], $lines );

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
					$self->_read_array( $array->[-1], [ @$indent, $indent2 ], $lines );
				}

			} elsif ( $lines->[0] =~ /^(\s*)\S/ ) {
				push @$array, { };
				$self->_read_hash( $array->[-1], [ @$indent, length("$1") ], $lines );

			} else {
				die \"CPAN::Meta::YAML failed to classify line '$lines->[0]'";
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
			die \"CPAN::Meta::YAML failed to classify line '$lines->[0]'";
		}
	}

	return 1;
}

# Parse an array
sub _read_hash {
	my ($self, $hash, $indent, $lines) = @_;

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
			die \"CPAN::Meta::YAML found bad indenting in line '$lines->[0]'";
		}

		# Get the key
		unless ( $lines->[0] =~ s/^\s*([^\'\" ][^\n]*?)\s*:(\s+(?:\#.*)?|$)// ) {
			if ( $lines->[0] =~ /^\s*[?\'\"]/ ) {
				die \"CPAN::Meta::YAML does not support a feature in line '$lines->[0]'";
			}
			die \"CPAN::Meta::YAML failed to classify line '$lines->[0]'";
		}
		my $key = $1;

		# Do we have a value?
		if ( length $lines->[0] ) {
			# Yes
			$hash->{$key} = $self->_read_scalar( shift(@$lines), [ @$indent, undef ], $lines );
		} else {
			# An indent
			shift @$lines;
			unless ( @$lines ) {
				$hash->{$key} = undef;
				return 1;
			}
			if ( $lines->[0] =~ /^(\s*)-/ ) {
				$hash->{$key} = [];
				$self->_read_array( $hash->{$key}, [ @$indent, length($1) ], $lines );
			} elsif ( $lines->[0] =~ /^(\s*)./ ) {
				my $indent2 = length("$1");
				if ( $indent->[-1] >= $indent2 ) {
					# Null hash entry
					$hash->{$key} = undef;
				} else {
					$hash->{$key} = {};
					$self->_read_hash( $hash->{$key}, [ @$indent, length($1) ], $lines );
				}
			}
		}
	}

	return 1;
}

# Save an object to a file
sub write {
	my $self = shift;
	my $file = shift or return $self->_error('No file name provided');

	# Write it to the file
	open( CFG, '>' . $file ) or return $self->_error(
		"Failed to open file '$file' for writing: $!"
		);
	print CFG $self->write_string;
	close CFG;

	return 1;
}

# Save an object to a string
sub write_string {
	my $self = shift;
	return '' unless @$self;

	# Iterate over the documents
	my $indent = 0;
	my @lines  = ();
	foreach my $cursor ( @$self ) {
		push @lines, '---';

		# An empty document
		if ( ! defined $cursor ) {
			# Do nothing

		# A scalar document
		} elsif ( ! ref $cursor ) {
			$lines[-1] .= ' ' . $self->_write_scalar( $cursor, $indent );

		# A list at the root
		} elsif ( ref $cursor eq 'ARRAY' ) {
			unless ( @$cursor ) {
				$lines[-1] .= ' []';
				next;
			}
			push @lines, $self->_write_array( $cursor, $indent, {} );

		# A hash at the root
		} elsif ( ref $cursor eq 'HASH' ) {
			unless ( %$cursor ) {
				$lines[-1] .= ' {}';
				next;
			}
			push @lines, $self->_write_hash( $cursor, $indent, {} );

		} else {
			Carp::croak("Cannot serialize " . ref($cursor));
		}
	}

	join '', map { "$_\n" } @lines;
}

sub _write_scalar {
	my $string = $_[1];
	return '~'  unless defined $string;
	return "''" unless length  $string;
	if ( $string =~ /[\x00-\x08\x0b-\x0d\x0e-\x1f\"\'\n]/ ) {
		$string =~ s/\\/\\\\/g;
		$string =~ s/"/\\"/g;
		$string =~ s/\n/\\n/g;
		$string =~ s/([\x00-\x1f])/\\$UNPRINTABLE[ord($1)]/g;
		return qq|"$string"|;
	}
	if ( $string =~ /(?:^\W|\s|:\z)/ or $QUOTE{$string} ) {
		return "'$string'";
	}
	return $string;
}

sub _write_array {
	my ($self, $array, $indent, $seen) = @_;
	if ( $seen->{refaddr($array)}++ ) {
		die "CPAN::Meta::YAML does not support circular references";
	}
	my @lines  = ();
	foreach my $el ( @$array ) {
		my $line = ('  ' x $indent) . '-';
		my $type = ref $el;
		if ( ! $type ) {
			$line .= ' ' . $self->_write_scalar( $el, $indent + 1 );
			push @lines, $line;

		} elsif ( $type eq 'ARRAY' ) {
			if ( @$el ) {
				push @lines, $line;
				push @lines, $self->_write_array( $el, $indent + 1, $seen );
			} else {
				$line .= ' []';
				push @lines, $line;
			}

		} elsif ( $type eq 'HASH' ) {
			if ( keys %$el ) {
				push @lines, $line;
				push @lines, $self->_write_hash( $el, $indent + 1, $seen );
			} else {
				$line .= ' {}';
				push @lines, $line;
			}

		} else {
			die "CPAN::Meta::YAML does not support $type references";
		}
	}

	@lines;
}

sub _write_hash {
	my ($self, $hash, $indent, $seen) = @_;
	if ( $seen->{refaddr($hash)}++ ) {
		die "CPAN::Meta::YAML does not support circular references";
	}
	my @lines  = ();
	foreach my $name ( sort keys %$hash ) {
		my $el   = $hash->{$name};
		my $line = ('  ' x $indent) . "$name:";
		my $type = ref $el;
		if ( ! $type ) {
			$line .= ' ' . $self->_write_scalar( $el, $indent + 1 );
			push @lines, $line;

		} elsif ( $type eq 'ARRAY' ) {
			if ( @$el ) {
				push @lines, $line;
				push @lines, $self->_write_array( $el, $indent + 1, $seen );
			} else {
				$line .= ' []';
				push @lines, $line;
			}

		} elsif ( $type eq 'HASH' ) {
			if ( keys %$el ) {
				push @lines, $line;
				push @lines, $self->_write_hash( $el, $indent + 1, $seen );
			} else {
				$line .= ' {}';
				push @lines, $line;
			}

		} else {
			die "CPAN::Meta::YAML does not support $type references";
		}
	}

	@lines;
}

# Set error
sub _error {
	$CPAN::Meta::YAML::errstr = $_[1];
	undef;
}

# Retrieve error
sub errstr {
	$CPAN::Meta::YAML::errstr;
}





#####################################################################
# YAML Compatibility

sub Dump {
	CPAN::Meta::YAML->new(@_)->write_string;
}

sub Load {
	my $self = CPAN::Meta::YAML->read_string(@_);
	unless ( $self ) {
		Carp::croak("Failed to load YAML document from string");
	}
	if ( wantarray ) {
		return @$self;
	} else {
		# To match YAML.pm, return the last document
		return $self->[-1];
	}
}

BEGIN {
	*freeze = *Dump;
	*thaw   = *Load;
}

sub DumpFile {
	my $file = shift;
	CPAN::Meta::YAML->new(@_)->write($file);
}

sub LoadFile {
	my $self = CPAN::Meta::YAML->read($_[0]);
	unless ( $self ) {
		Carp::croak("Failed to load YAML document from '" . ($_[0] || '') . "'");
	}
	if ( wantarray ) {
		return @$self;
	} else {
		# Return only the last document to match YAML.pm, 
		return $self->[-1];
	}
}





#####################################################################
# Use Scalar::Util if possible, otherwise emulate it

BEGIN {
	local $@;
	eval {
		require Scalar::Util;
	};
	my $v = eval("$Scalar::Util::VERSION") || 0;
	if ( $@ or $v < 1.18 ) {
		eval <<'END_PERL';
# Scalar::Util failed to load or too old
sub refaddr {
	my $pkg = ref($_[0]) or return undef;
	if ( !! UNIVERSAL::can($_[0], 'can') ) {
		bless $_[0], 'Scalar::Util::Fake';
	} else {
		$pkg = undef;
	}
	"$_[0]" =~ /0x(\w+)/;
	my $i = do { local $^W; hex $1 };
	bless $_[0], $pkg if defined $pkg;
	$i;
}
END_PERL
	} else {
		*refaddr = *Scalar::Util::refaddr;
	}
}

1;



=pod

=head1 NAME

CPAN::Meta::YAML - Read and write a subset of YAML for CPAN Meta files

=head1 VERSION

version 0.008

=head1 SYNOPSIS

    use CPAN::Meta::YAML;

    # reading a META file
    open $fh, "<:utf8", "META.yml";
    $yaml_text = do { local $/; <$fh> };
    $yaml = CPAN::Meta::YAML->read_string($yaml_text)
      or die CPAN::Meta::YAML->errstr;

    # finding the metadata
    $meta = $yaml->[0];

    # writing a META file
    $yaml_text = $yaml->write_string
      or die CPAN::Meta::YAML->errstr;
    open $fh, ">:utf8", "META.yml";
    print $fh $yaml_text;

=head1 DESCRIPTION

This module implements a subset of the YAML specification for use in reading
and writing CPAN metadata files like F<META.yml> and F<MYMETA.yml>.  It should
not be used for any other general YAML parsing or generation task.

NOTE: F<META.yml> (and F<MYMETA.yml>) files should be UTF-8 encoded.  Users are
responsible for proper encoding and decoding.  In particular, the C<read> and
C<write> methods do B<not> support UTF-8 and should not be used.

=head1 SUPPORT

This module is currently derived from L<YAML::Tiny> by Adam Kennedy.  If
there are bugs in how it parses a particular META.yml file, please file
a bug report in the YAML::Tiny bugtracker:
L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=YAML-Tiny>

=head1 SEE ALSO

L<YAML::Tiny>, L<YAML>, L<YAML::XS>

=for :stopwords cpan testmatrix url annocpan anno bugtracker rt cpants kwalitee diff irc mailto metadata placeholders metacpan

=head1 SUPPORT

=head2 Bugs / Feature Requests

Please report any bugs or feature requests through the issue tracker
at L<http://rt.cpan.org/Public/Dist/Display.html?Name=CPAN-Meta-YAML>.
You will be notified automatically of any progress on your issue.

=head2 Source Code

This is open source software.  The code repository is available for
public review and contribution under the terms of the license.

L<https://github.com/dagolden/cpan-meta-yaml>

  git clone https://github.com/dagolden/cpan-meta-yaml.git

=head1 AUTHORS

=over 4

=item *

Adam Kennedy <adamk@cpan.org>

=item *

David Golden <dagolden@cpan.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2010 by Adam Kennedy.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut


__END__


# ABSTRACT: Read and write a subset of YAML for CPAN Meta files


