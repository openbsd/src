class PerlInterpreter {
    static boolean initted = false;

    public native void init(String s);
    public native void eval(String s);

//    public native long op(long i);

    public PerlInterpreter fetch () {
	if (!initted) {
	    init("$JPL::DEBUG = $ENV{JPLDEBUG}");
	    initted = true;
	}
	return this;
    }

    static {
	System.loadLibrary("PerlInterpreter");
    }
}

