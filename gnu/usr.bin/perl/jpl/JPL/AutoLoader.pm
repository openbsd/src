package JPL::AutoLoader;

use strict;

use vars qw(@ISA @EXPORT $AUTOLOAD);

use Exporter;
@ISA = "Exporter";
@EXPORT = ("AUTOLOAD", "getmeth");

my %callmethod = (
    V => 'Void',
    Z => 'Boolean',
    B => 'Byte',
    C => 'Char',
    S => 'Short',
    I => 'Int',
    J => 'Long',
    F => 'Float',
    D => 'Double',
);

# A lookup table to convert the data types that Java
# developers are used to seeing into the JNI-mangled
# versions.
#
# bjepson 13 August 1997
#
my %type_table = (
    'void'    => 'V',
    'boolean' => 'Z',
    'byte'    => 'B',
    'char'    => 'C',
    'short'   => 'S',
    'int'     => 'I',
    'long'    => 'J',
    'float'   => 'F',
    'double'  => 'D'
);

# A cache for method ids.
#
# bjepson 13 August 1997
#
my %MID_CACHE;

# A cache for methods.
#
# bjepson 13 August 1997
#
my %METHOD_CACHE;

use JNI;

# XXX We're assuming for the moment that method ids are persistent...

sub AUTOLOAD {

    print "AUTOLOAD $AUTOLOAD(@_)\n" if $JPL::DEBUG;
    my ($classname, $methodsig) = $AUTOLOAD =~ /^(.*)::(.*)/;
    print "class = $classname, method = $methodsig\n" if $JPL::DEBUG;

    if ($methodsig eq "DESTROY") {
        print "sub $AUTOLOAD {}\n" if $JPL::DEBUG;
        eval "sub $AUTOLOAD {}";
        return;
    }

    (my $jclassname = $classname) =~ s/^JPL:://;
    $jclassname =~ s{::}{/}g;
    my $class = JNI::FindClass($jclassname)
        or die "Can't find Java class $jclassname\n";

    # This method lookup allows the user to pass in
    # references to two array that contain the input and
    # output data types of the method.
    #
    # bjepson 13 August 1997
    #
    my ($methodname, $sig, $retsig, $slow_way);
    if (ref $_[1] eq 'ARRAY' && ref $_[2] eq 'ARRAY') {

	$slow_way = 1;

        # First we strip out the input and output args.
	#
        my ($in,$out) = splice(@_, 1, 2);

        # let's mangle up the input argument types.
        #
        my @in  = jni_mangle($in);

        # if they didn't hand us any output values types, make
        # them void by default.
        #
        unless (@{ $out }) {
            $out = ['void'];
        }

        # mangle the output types
        #
        my @out = jni_mangle($out);

        $methodname = $methodsig;
        $retsig     = join("", @out);
        $sig        = "(" . join("", @in) . ")" . $retsig;

    } else {

        ($methodname, $sig) = split /__/, $methodsig, 2;
        $sig ||= "__V";                # default is void return

        # Now demangle the signature.

        $sig =~ s/_3/[/g;
        $sig =~ s/_2/;/g;
        my $tmp;
        $sig =~ s{
            (s|L[^;]*;)
        }{
	    $1 eq 's'
		? "Ljava/lang/String;"
		: (($tmp = $1) =~ tr[_][/], $tmp)
        }egx;
        if ($sig =~ s/(.*)__(.*)/($1)$2/) {
            $retsig = $2;
        }
        else {                        # void return is assumed
            $sig = "($sig)V";
            $retsig = "V";
        }
        $sig =~ s/_1/_/g;
    }
    print "sig = $sig\n" if $JPL::DEBUG;

    # Now look up the method's ID somehow or other.
    #
    $methodname = "<init>" if $methodname eq 'new';
    my $mid;

    # Added a method id cache to compensate for avoiding
    # Perl's method cache...
    #
    if ($MID_CACHE{qq[$classname:$methodname:$sig]}) {

        $mid = $MID_CACHE{qq[$classname:$methodname:$sig]};
        print "got method " . ($mid + 0) . " from cache.\n" if $JPL::DEBUG;

    } elsif (ref $_[0] or $methodname eq '<init>') {

        # Look up an instance method or a constructor
        #
        $mid = JNI::GetMethodID($class, $methodname, $sig);

    } else {

        # Look up a static method
        #
        $mid = JNI::GetStaticMethodID($class, $methodname, $sig);

    }

    # Add this method to the cache.
    #
    # bjepson 13 August 1997
    #
    $MID_CACHE{qq[$classname:$methodname:$sig]} = $mid if $slow_way;

    if ($mid == 0) {

        JNI::ExceptionClear();
        # Could do some guessing here on return type...
        die "Can't get method id for $AUTOLOAD($sig)\n";

    }

    print "mid = ", $mid + 0, ", $mid\n" if $JPL::DEBUG;
    my $rettype = $callmethod{$retsig} || "Object";
    print "*** rettype = $rettype\n" if $JPL::DEBUG;

    my $blesspack;
    no strict 'refs';
    if ($rettype eq "Object") {
        $blesspack = $retsig;
        $blesspack =~ s/^L//;
        $blesspack =~ s/;$//;
        $blesspack =~ s#/#::#g;
        print "*** Some sort of wizardry...\n" if $JPL::DEBUG;
        print %{$blesspack . "::"}, "\n" if $JPL::DEBUG;
        print defined %{$blesspack . "::"}, "\n" if $JPL::DEBUG;
        if (not defined %{$blesspack . "::"}) {
            #if ($blesspack eq "java::lang::String") {
            if ($blesspack =~ /java::/) {
                eval <<"END" . <<'ENDQ';
package $blesspack;
END
use JPL::AutoLoader;
use overload
        '""' => sub { JNI::GetStringUTFChars($_[0]) },
        '0+' => sub { 0 + "$_[0]" },
        fallback => 1;
ENDQ
            }
            else {
                eval <<"END";
package $blesspack;
use JPL::AutoLoader;
END
            }
        }
    }

    # Finally, call the method.  Er, somehow...
    #
    my $METHOD;

    my $real_mid = $mid + 0; # weird overloading that I
                             # don't understand ?!
    if (ref ${$METHOD_CACHE{qq[$real_mid]}} eq 'CODE') {

        $METHOD = ${$METHOD_CACHE{qq[$real_mid]}};
        print qq[Pulled $classname, $methodname, $sig from cache.\n] if $JPL::DEBUG;

    } elsif ($methodname eq "<init>") {
        $METHOD = sub {
            my $self = shift;
	    my $class = JNI::FindClass($jclassname);
            bless $class->JNI::NewObjectA($mid, \@_), $classname;
        };
    }
    elsif (ref $_[0]) {
        if ($blesspack) {
            $METHOD = sub {
                my $self = shift;
                if (ref $self eq $classname) {
                    my $callmethod = "JNI::Call${rettype}MethodA";
                    bless $self->$callmethod($mid, \@_), $blesspack;
                }
                else {
                    my $callmethod = "JNI::CallNonvirtual${rettype}MethodA";
                    bless $self->$callmethod($class, $mid, \@_), $blesspack;
                }
            };
        }
        else {
            $METHOD = sub {
                my $self = shift;
                if (ref $self eq $classname) {
                    my $callmethod = "JNI::Call${rettype}MethodA";
                    $self->$callmethod($mid, \@_);
                }
                else {
                    my $callmethod = "JNI::CallNonvirtual${rettype}MethodA";
                    $self->$callmethod($class, $mid, \@_);
                }
            };
        }
    }
    else {
        my $callmethod = "JNI::CallStatic${rettype}MethodA";
        if ($blesspack) {
            $METHOD = sub {
                my $self = shift;
                bless $class->$callmethod($mid, \@_), $blesspack;
            };
        }
        else {
            $METHOD = sub {
                my $self = shift;
                $class->$callmethod($mid, \@_);
            };
        }
    }
    if ($slow_way) {
	$METHOD_CACHE{qq[$real_mid]} = \$METHOD;
	&$METHOD;
    }
    else {
	*$AUTOLOAD = $METHOD;
	goto &$AUTOLOAD;
    }
}

sub jni_mangle {

    my $arr = shift;
    my @ret;

    foreach my $arg (@{ $arr }) {

        my $ret;

        # Count the dangling []s.
        #
	$ret = '[' x $arg =~ s/\[\]//g;

        # Is it a primitive type?
        #
        if ($type_table{$arg}) {
            $ret .= $type_table{$arg};
        } else {
            # some sort of class
            #
            $arg =~ s#\.#/#g;
            $ret .= "L$arg;";
        }
        push @ret, $ret;

    }

    return @ret;

}

sub getmeth {
    my ($meth, $in, $out) = @_;
    my @in  = jni_mangle($in);

    # if they didn't hand us any output values types, make
    # them void by default.
    #
    unless ($out and @$out) {
	$out = ['void'];
    }

    # mangle the output types
    #
    my @out = jni_mangle($out);

    my $sig        = join("", '#', @in, '#', @out);
    $sig =~ s/_/_1/g;
    my $tmp;
    $sig =~ s{
	(L[^;]*;)
    }{
	($tmp = $1) =~ tr[/][_], $tmp
    }egx;
    $sig =~ s{Ljava/lang/String;}{s}g;
    $sig =~ s/;/_2/g;
    $sig =~ s/\[/_3/g;
    $sig =~ s/#/__/g;
    $meth . $sig;
}

{
    package java::lang::String;
    use overload
	'""' => sub { JNI::GetStringUTFChars($_[0]) },
	'0+' => sub { 0 + "$_[0]" },
	fallback => 1;
}
1;
