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

$VERSION = '0.01';

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
    $ENV{JAVA_HOME} ||= "/usr/local/java";

    my ($arch, @CLASSPATH);
    if ($^O eq 'MSWin32') {

        $arch = 'MSWin32' unless -d "$ENV{JAVA_HOME}/lib/$arch";
        @CLASSPATH = split(/;/, $ENV{CLASSPATH});
        @CLASSPATH = "." unless @CLASSPATH;
        push @CLASSPATH,
	    "$ENV{JAVA_HOME}\\classes",
	    "$ENV{JAVA_HOME}\\lib\\classes.zip";

        $ENV{CLASSPATH} = join(';', @CLASSPATH);
        $ENV{THREADS_TYPE} ||= "green_threads";

        $JAVALIB = "$ENV{JAVA_HOME}/lib/$arch/$ENV{THREADS_TYPE}";
        $ENV{LD_LIBRARY_PATH} .= ":$JAVALIB";

        push @JVM_ARGS, "classpath", $ENV{CLASSPATH};
        print "JVM_ARGS=@JVM_ARGS!\n";
        $JVM = GetJavaVM("$JAVALIB/javai.dll",@JVM_ARGS);

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
        print "JVM_ARGS=@JVM_ARGS!\n";
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
