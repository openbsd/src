package OS2::Process;

require Exporter;
require DynaLoader;
require AutoLoader;

@ISA = qw(Exporter DynaLoader);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw(
	P_BACKGROUND
	P_DEBUG
	P_DEFAULT
	P_DETACH
	P_FOREGROUND
	P_FULLSCREEN
	P_MAXIMIZE
	P_MINIMIZE
	P_NOCLOSE
	P_NOSESSION
	P_NOWAIT
	P_OVERLAY
	P_PM
	P_QUOTE
	P_SESSION
	P_TILDE
	P_UNRELATED
	P_WAIT
	P_WINDOWED
);
sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.  If a constant is not found then control is passed
    # to the AUTOLOAD in AutoLoader.

    local($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {
	if ($! =~ /Invalid/) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
	    ($pack,$file,$line) = caller;
	    die "Your vendor has not defined OS2::Process macro $constname, used at $file line $line.
";
	}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

bootstrap OS2::Process;

# Preloaded methods go here.

# Autoload methods go after __END__, and are processed by the autosplit program.

1;
__END__

=head1 NAME

OS2::Process - exports constants for system() call on OS2.

=head1 SYNOPSIS

    use OS2::Process;
    $pid = system(P_PM+P_BACKGROUND, "epm.exe");

=head1 DESCRIPTION

the builtin function system() under OS/2 allows an optional first
argument which denotes the mode of the process. Note that this argument is
recognized only if it is strictly numerical.

You can use either one of the process modes:

	P_WAIT (0)	= wait until child terminates (default)
	P_NOWAIT	= do not wait until child terminates
	P_SESSION	= new session
	P_DETACH	= detached
	P_PM		= PM program

and optionally add PM and session option bits:

	P_DEFAULT (0)	= default
	P_MINIMIZE	= minimized
	P_MAXIMIZE	= maximized
	P_FULLSCREEN	= fullscreen (session only)
	P_WINDOWED	= windowed (session only)

	P_FOREGROUND	= foreground (if running in foreground)
	P_BACKGROUND	= background

	P_NOCLOSE	= don't close window on exit (session only)

	P_QUOTE		= quote all arguments
	P_TILDE		= MKS argument passing convention
	P_UNRELATED	= do not kill child when father terminates

=head1 AUTHOR

Andreas Kaiser <ak@ananke.s.bawue.de>.

=head1 SEE ALSO

C<spawn*>() system calls.

=cut
