package Fcntl;

=head1 NAME

Fcntl - load the C Fcntl.h defines

=head1 SYNOPSIS

    use Fcntl;

=head1 DESCRIPTION

This module is just a translation of the C F<fnctl.h> file.
Unlike the old mechanism of requiring a translated F<fnctl.ph>
file, this uses the B<h2xs> program (see the Perl source distribution)
and your native C compiler.  This means that it has a 
far more likely chance of getting the numbers right.

=head1 NOTE

Only C<#define> symbols get translated; you must still correctly
pack up your own arguments to pass as args for locking functions, etc.

=cut

use vars qw($VERSION @ISA @EXPORT @EXPORT_OK $AUTOLOAD);

require Exporter;
use AutoLoader;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);
$VERSION = "1.00";
# Items to export into callers namespace by default
# (move infrequently used names to @EXPORT_OK below)
@EXPORT =
  qw(
     F_DUPFD F_GETFD F_GETLK F_SETFD F_GETFL F_SETFL F_SETLK F_SETLKW
     FD_CLOEXEC F_RDLCK F_UNLCK F_WRLCK
     O_CREAT O_EXCL O_NOCTTY O_TRUNC
     O_APPEND O_NONBLOCK
     O_NDELAY
     O_RDONLY O_RDWR O_WRONLY
     );
# Other items we are prepared to export if requested
@EXPORT_OK = qw(
);

sub AUTOLOAD {
    my($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    my $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {
	if ($! =~ /Invalid/) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
	    my ($pack,$file,$line) = caller;
	    die "Your vendor has not defined Fcntl macro $constname, used at $file line $line.
";
	}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

bootstrap Fcntl $VERSION;

# Preloaded methods go here.  Autoload methods go after __END__, and are
# processed by the autosplit program.
package Fcntl; # return to package Fcntl so AutoSplit is happy
1;
__END__
