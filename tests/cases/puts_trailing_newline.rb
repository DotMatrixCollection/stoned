$stdout.sync = true

# puts with string ending in \n should not add another newline

# Normal string: puts adds \n
puts "hello"
puts "world"

# String ending in \n: puts does NOT add another \n
puts "line with newline\n"
puts "next"

# "\n" alone: prints one blank line (not two)
puts "\n"
puts "after blank"

# Array elements: same rule
puts ["a\n", "b", "c\n"]
