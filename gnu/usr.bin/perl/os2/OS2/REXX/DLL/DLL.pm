package OS2::DLL;

use Carp;
use DynaLoader;

@ISA = qw(DynaLoader);

sub AUTOLOAD {
    $AUTOLOAD =~ /^OS2::DLL::.+::(.+)$/
      or confess("Undefined subroutine &$AUTOLOAD called");
    return undef if $1 eq "DESTROY";
    $_[0]->find($1)
      or confess("Can't find entry '$1' to DLL '$_[0]->{File}': $^E");
    goto &$AUTOLOAD;
}

@libs = split(/;/, $ENV{'PERL5REXX'} || $ENV{'PERLREXX'} || $ENV{'LIBPATH'} || $ENV{'PATH'});
%dlls = ();

# Preloaded methods go here.  Autoload methods go after __END__, and are
# processed by the autosplit program.

# Cannot autoload, the autoloader is used for the REXX functions.

sub load
{
	confess 'Usage: load OS2::DLL <file> [<dirs>]' unless $#_ >= 1;
	my ($class, $file, @where) = (@_, @libs);
	return $dlls{$file} if $dlls{$file};
	my $handle;
	foreach (@where) {
		$handle = DynaLoader::dl_load_file("$_/$file.dll");
		last if $handle;
	}
	$handle = DynaLoader::dl_load_file($file) unless $handle;
	return undef unless $handle;
	my $packs = $INC{'OS2/REXX.pm'} ? 'OS2::DLL OS2::REXX' : 'OS2::DLL';
	eval <<EOE or die "eval package $@";
package OS2::DLL::$file; \@ISA = qw($packs);
sub AUTOLOAD {
  \$OS2::DLL::AUTOLOAD = \$AUTOLOAD;
  goto &OS2::DLL::AUTOLOAD;
}
1;
EOE
	return $dlls{$file} = 
	  bless {Handle => $handle, File => $file, Queue => 'SESSION' },
		"OS2::DLL::$file";
}

sub find
{
	my $self   = shift;
	my $file   = $self->{File};
	my $handle = $self->{Handle};
	my $prefix = exists($self->{Prefix}) ? $self->{Prefix} : "";
	my $queue  = $self->{Queue};
	foreach (@_) {
		my $name = "OS2::DLL::${file}::$_";
		next if defined(&$name);
		my $addr = DynaLoader::dl_find_symbol($handle, uc $prefix.$_)
		        || DynaLoader::dl_find_symbol($handle, $prefix.$_)
			or return 0;
		eval <<EOE or die "eval sub";
package OS2::DLL::$file;
sub $_ {
  shift;
  OS2::DLL::_call('$_', $addr, '$queue', \@_);
}
1;
EOE
	}
	return 1;
}

bootstrap OS2::DLL;

1;
__END__

=head1 NAME

OS2::DLL - access to DLLs with REXX calling convention.

=head2 NOTE

When you use this module, the REXX variable pool is not available.

See documentation of L<OS2::REXX> module if you need the variable pool.

=head1 SYNOPSIS

	use OS2::DLL;
	$emx_dll = OS2::DLL->load('emx');
	$emx_version = $emx_dll->emx_revision();

=head1 DESCRIPTION

=head2 Load REXX DLL

	$dll = load OS2::DLL NAME [, WHERE];

NAME is DLL name, without path and extension.

Directories are searched WHERE first (list of dirs), then environment
paths PERL5REXX, PERLREXX, PATH or, as last resort, OS/2-ish search 
is performed in default DLL path (without adding paths and extensions).

The DLL is not unloaded when the variable dies.

Returns DLL object reference, or undef on failure.

=head2 Check for functions (optional):

	BOOL = $dll->find(NAME [, NAME [, ...]]);

Returns true if all functions are available.

=head2 Call external REXX function:

	$dll->function(arguments);

Returns the return string if the return code is 0, else undef.
Dies with error message if the function is not available.

=head1 ENVIRONMENT

If C<PERL_REXX_DEBUG> is set, emits debugging output.  Looks for DLLs
in C<PERL5REXX>, C<PERLREXX>, C<PATH>.

=head1 AUTHOR

Extracted by Ilya Zakharevich ilya@math.ohio-state.edu from L<OS2::REXX>
written by Andreas Kaiser ak@ananke.s.bawue.de.

=cut
