package JNI;

use strict;
use Carp;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK $AUTOLOAD $JVM @JVM_ARGS $JAVALIB);

require Exporter;
require DynaLoader;
require AutoLoader;

@ISA = qw(Exporter DynaLoader);

@EXPORT = qw(
	JNI_ABORT
	JNI_COMMIT
	JNI_ERR
	JNI_FALSE
	JNI_H
	JNI_OK
	JNI_TRUE
	GetVersion
	DefineClass
	FindClass
	GetSuperclass
	IsAssignableFrom
	Throw
	ThrowNew
	ExceptionOccurred
	ExceptionDescribe
	ExceptionClear
	FatalError
	NewGlobalRef
	DeleteGlobalRef
	DeleteLocalRef
	IsSameObject
	AllocObject
	NewObject
	NewObjectA
	GetObjectClass
	IsInstanceOf
	GetMethodID
	CallObjectMethod
	CallObjectMethodA
	CallBooleanMethod
	CallBooleanMethodA
	CallByteMethod
	CallByteMethodA
	CallCharMethod
	CallCharMethodA
	CallShortMethod
	CallShortMethodA
	CallIntMethod
	CallIntMethodA
	CallLongMethod
	CallLongMethodA
	CallFloatMethod
	CallFloatMethodA
	CallDoubleMethod
	CallDoubleMethodA
	CallVoidMethod
	CallVoidMethodA
	CallNonvirtualObjectMethod
	CallNonvirtualObjectMethodA
	CallNonvirtualBooleanMethod
	CallNonvirtualBooleanMethodA
	CallNonvirtualByteMethod
	CallNonvirtualByteMethodA
	CallNonvirtualCharMethod
	CallNonvirtualCharMethodA
	CallNonvirtualShortMethod
	CallNonvirtualShortMethodA
	CallNonvirtualIntMethod
	CallNonvirtualIntMethodA
	CallNonvirtualLongMethod
	CallNonvirtualLongMethodA
	CallNonvirtualFloatMethod
	CallNonvirtualFloatMethodA
	CallNonvirtualDoubleMethod
	CallNonvirtualDoubleMethodA
	CallNonvirtualVoidMethod
	CallNonvirtualVoidMethodA
	GetFieldID
	GetObjectField
	GetBooleanField
	GetByteField
	GetCharField
	GetShortField
	GetIntField
	GetLongField
	GetFloatField
	GetDoubleField
	SetObjectField
	SetBooleanField
	SetByteField
	SetCharField
	SetShortField
	SetIntField
	SetLongField
	SetFloatField
	SetDoubleField
	GetStaticMethodID
	CallStaticObjectMethod
	CallStaticObjectMethodA
	CallStaticBooleanMethod
	CallStaticBooleanMethodA
	CallStaticByteMethod
	CallStaticByteMethodA
	CallStaticCharMethod
	CallStaticCharMethodA
	CallStaticShortMethod
	CallStaticShortMethodA
	CallStaticIntMethod
	CallStaticIntMethodA
	CallStaticLongMethod
	CallStaticLongMethodA
	CallStaticFloatMethod
	CallStaticFloatMethodA
	CallStaticDoubleMethod
	CallStaticDoubleMethodA
	CallStaticVoidMethod
	CallStaticVoidMethodA
	GetStaticFieldID
	GetStaticObjectField
	GetStaticBooleanField
	GetStaticByteField
	GetStaticCharField
	GetStaticShortField
	GetStaticIntField
	GetStaticLongField
	GetStaticFloatField
	GetStaticDoubleField
	SetStaticObjectField
	SetStaticBooleanField
	SetStaticByteField
	SetStaticCharField
	SetStaticShortField
	SetStaticIntField
	SetStaticLongField
	SetStaticFloatField
	SetStaticDoubleField
	NewString
	GetStringLength
	GetStringChars
	NewStringUTF
	GetStringUTFLength
	GetStringUTFChars
	GetArrayLength
	NewObjectArray
	GetObjectArrayElement
	SetObjectArrayElement
	NewBooleanArray
	NewByteArray
	NewCharArray
	NewShortArray
	NewIntArray
	NewLongArray
	NewFloatArray
	NewDoubleArray
	GetBooleanArrayElements
	GetByteArrayElements
	GetCharArrayElements
	GetShortArrayElements
	GetIntArrayElements
	GetLongArrayElements
	GetFloatArrayElements
	GetDoubleArrayElements
	GetBooleanArrayRegion
	GetByteArrayRegion
	GetCharArrayRegion
	GetShortArrayRegion
	GetIntArrayRegion
	GetLongArrayRegion
	GetFloatArrayRegion
	GetDoubleArrayRegion
	SetBooleanArrayRegion
	SetByteArrayRegion
	SetCharArrayRegion
	SetShortArrayRegion
	SetIntArrayRegion
	SetLongArrayRegion
	SetFloatArrayRegion
	SetDoubleArrayRegion
	RegisterNatives
	UnregisterNatives
	MonitorEnter
	MonitorExit
	GetJavaVM
);

$VERSION = '0.2';

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.  If a constant is not found then control is passed
    # to the AUTOLOAD in AutoLoader.

    my $constname;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    my $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {
	if ($! =~ /Invalid/) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
		croak "Your vendor has not defined JNI macro $constname";
	}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

bootstrap JNI $VERSION;

if (not $JPL::_env_) {
    # Note that only Kaffe support only cares about what JNI::Config says
    use JNI::Config qw($KAFFE $LD_LIBRARY_PATH $CLASS_HOME $LIB_HOME $JAVA_LIB);

    # Win32 and Sun JDK pay attention to $ENV{JAVA_HOME}; Kaffe doesn't
    $ENV{JAVA_HOME} ||= "/usr/local/java";

    my ($arch, @CLASSPATH);
    if ($^O eq 'MSWin32' and (! $JNI::Config::KAFFE) ) {

        $arch = 'MSWin32' unless -d "$ENV{JAVA_HOME}/lib/$arch";
        @CLASSPATH = split(/;/, $ENV{CLASSPATH});
        @CLASSPATH = "." unless @CLASSPATH;
        push @CLASSPATH,
	    "$ENV{JAVA_HOME}\\classes",
	    "$ENV{JAVA_HOME}\\lib\\classes.zip",
	    # MSR - added for JDK 1.3
	    "$ENV{JAVA_HOME}\\jre\\lib\\rt.jar",
	    # MSR - added to find Closer.class
	    '.';

        $ENV{CLASSPATH} = join(';', @CLASSPATH);
        $ENV{THREADS_TYPE} ||= "green_threads";

        #$JAVALIB = "$ENV{JAVA_HOME}/lib/$arch/$ENV{THREADS_TYPE}";
        # MSR  - changed above for JDK 1.3
        $JAVALIB = "$ENV{JAVA_HOME}/lib/";

        $ENV{LD_LIBRARY_PATH} .= ":$JAVALIB";

        push @JVM_ARGS, "classpath", $ENV{CLASSPATH};
        print "JVM_ARGS=@JVM_ARGS!\n" if $JPL::DEBUG;
        $JVM = GetJavaVM("$JAVALIB/javai.dll",@JVM_ARGS);
    } elsif ($^O eq 'MSWin32' and $JNI::Config::KAFFE) {
        croak "Kaffe is not yet supported on MSWin32 platform!";
    } elsif ($JNI::Config::KAFFE) {
        # The following code has to build a classpath for us.  It would be
        # better if we could have *both* a classpath and a classhome, and
        # not have to "guess" at the classpath like this.  We should be able
        # to send in, say, a classpath of ".", and classhome of
        # ".../share/kaffe", and have it build the right classpath.  That
        # doesn't work.  The function initClasspath() in findInJar.c in the
        # Kaffe source says: "Oh, you have a classpath, well forget
        # classhome!"  This seems brain-dead to me.  But, anyway, that's why
        # I don't use the classhome option on GetJavaVM.  I have to build
        # the classpath by hand.  *sigh*
        #                                            --  bkuhn

        my $classpath = $ENV{CLASSPATH} || ".";
        my %classCheck;
        @classCheck{split(/\s*:\s*/, $classpath)} = 1;
        foreach my $jarFile (qw(Klasses.jar comm.jar pjava.jar
                                        tools.jar microsoft.jar rmi.jar)) {
          $classpath .= ":$JNI::Config::CLASS_HOME/$jarFile"
            unless defined $classCheck{"$JNI::Config::CLASS_HOME/$jarFile"};
            # Assume that if someone else already put these here, they knew
            # what they were doing and have the order right.
        }
        $classpath = ".:$classpath" unless defined $classCheck{"."};

        $ENV{CLASSPATH} = $classpath;  # Not needed for GetJavaVM(), since
                                       # we pass it in as a JVM option, but
                                       # something else might expect it.
                                       # (also see comment above)
        print STDERR "bkuhn: JNI classpath=$classpath\n";
        unshift(@JVM_ARGS, "classpath", $classpath,
                "libraryhome", $JNI::Config::LIB_HOME);

                #   The following line is useless; see comment above.
                #  "classhome", $JNI::Config::CLASS_HOME);

        $JVM = GetJavaVM($JNI::Config::JAVA_LIB, @JVM_ARGS);
    } else {
        chop($arch = `uname -p`);
        chop($arch = `uname -m`) unless -d "$ENV{JAVA_HOME}/lib/$arch";

        @CLASSPATH = split(/:/, $ENV{CLASSPATH});
        @CLASSPATH = "." unless @CLASSPATH;
        push @CLASSPATH,
	    "$ENV{JAVA_HOME}/classes",
	    "$ENV{JAVA_HOME}/lib/classes.zip";
        $ENV{CLASSPATH} = join(':', @CLASSPATH);

        $ENV{THREADS_TYPE} ||= "green_threads";

        $JAVALIB = "$ENV{JAVA_HOME}/lib/$arch/$ENV{THREADS_TYPE}";
        $ENV{LD_LIBRARY_PATH} .= ":$JAVALIB";
        push @JVM_ARGS, "classpath", $ENV{CLASSPATH};
        print "JVM_ARGS=@JVM_ARGS!\n" if $JPL::DEBUG;
        $JVM = GetJavaVM("$JAVALIB/libjava.so",@JVM_ARGS);
    }
}

1;
__END__

=head1 NAME

JNI - Perl encapsulation of the Java Native Interface

=head1 SYNOPSIS

  use JNI;

=head1 DESCRIPTION

=head1 Exported constants

  JNI_ABORT
  JNI_COMMIT
  JNI_ERR
  JNI_FALSE
  JNI_H
  JNI_OK
  JNI_TRUE


=head1 AUTHOR

Copyright 1998, O'Reilly & Associates, Inc.

This package may be copied under the same terms as Perl itself.

=head1 SEE ALSO

perl(1).

=cut
