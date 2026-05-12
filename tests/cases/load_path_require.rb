$LOAD_PATH << "tests/fixtures/load_path"

puts require("basic")
puts require("basic")
puts require("subdir/nested")
puts require("./subdir/./nested")
puts LOAD_PATH_VALUE
puts LOAD_PATH_NESTED_VALUE
