function test1(array) { array["test"] = "data" }
function test2(array) { return(array["test"]) }
BEGIN { test1(foo); print "data: " test2(foo) }
