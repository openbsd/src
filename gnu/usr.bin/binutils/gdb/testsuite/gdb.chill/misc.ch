misc_tests : MODULE;

DCL otto INT := 42;

DCL foo STRUCT (l LONG, c CHAR, b BOOL, s CHARS(3));

dummyfunc: PROC();
END dummyfunc;

dummyfunc();

END misc_tests;
