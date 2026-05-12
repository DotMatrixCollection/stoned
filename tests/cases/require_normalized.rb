puts require("./tests/fixtures/require/basic")
puts require("tests/fixtures/require/./basic")
puts require("tests/fixtures/require/subdir/../basic")
puts require("/home/matrix9180/Projects/newruby/stoned/tests/fixtures/require/basic")
puts REQUIRE_VALUE
