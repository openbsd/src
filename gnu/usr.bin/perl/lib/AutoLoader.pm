package AutoLoader;
use Carp;
$DB::sub = $DB::sub;	# Avoid warning

=head1 NAME

AutoLoader - load functions only on demand

=head1 SYNOPSIS

    package FOOBAR;
    use Exporter;
    use AutoLoader;
    @ISA = (Exporter, AutoLoader);

=head1 DESCRIPTION

This module tells its users that functions in the FOOBAR package are to be
autoloaded from F<auto/$AUTOLOAD.al>.  See L<perlsub/"Autoloading">.

=cut

AUTOLOAD {
    my $name = "auto/$AUTOLOAD.al";
    $name =~ s#::#/#g;
    eval {require $name};
    if ($@) {
	# The load might just have failed because the filename was too
	# long for some old SVR3 systems which treat long names as errors.
	# If we can succesfully truncate a long name then it's worth a go.
	# There is a slight risk that we could pick up the wrong file here
	# but autosplit should have warned about that when splitting.
	if ($name =~ s/(\w{12,})\.al$/substr($1,0,11).".al"/e){
	    eval {require $name};
	}
	elsif ($AUTOLOAD =~ /::DESTROY$/) {
	    # eval "sub $AUTOLOAD {}";
	    *$AUTOLOAD = sub {};
	}
	if ($@){
	    $@ =~ s/ at .*\n//;
	    croak $@;
	}
    }
    $DB::sub = $AUTOLOAD;	# Now debugger know where we are.
    goto &$AUTOLOAD;
}
                            
sub import {
    my ($callclass, $callfile, $callline,$path,$callpack) = caller(0);
    ($callpack = $callclass) =~ s#::#/#;
    # Try to find the autosplit index file.  Eg., if the call package
    # is POSIX, then $INC{POSIX.pm} is something like
    # '/usr/local/lib/perl5/POSIX.pm', and the autosplit index file is in
    # '/usr/local/lib/perl5/auto/POSIX/autosplit.ix', so we require that.
    #
    # However, if @INC is a relative path, this might not work.  If,
    # for example, @INC = ('lib'), then
    # $INC{POSIX.pm} is 'lib/POSIX.pm', and we want to require
    # 'auto/POSIX/autosplit.ix' (without the leading 'lib').
    #
    if (defined($path = $INC{$callpack . '.pm'})) {
	# Try absolute path name.
	$path =~ s#^(.*)$callpack\.pm$#$1auto/$callpack/autosplit.ix#;
	eval { require $path; };
	# If that failed, try relative path with normal @INC searching.
	if ($@) {
	    $path ="auto/$callpack/autosplit.ix";
	    eval { require $path; };
	}
	carp $@ if ($@);  
    } 
}

1;
