$stdout.sync = true

# String#upto iterates from self through end using succ

# Simple alpha range
chars = []
"a".upto("e") { |c| chars << c }
puts chars.inspect  # ["a", "b", "c", "d", "e"]

# Numeric string range
nums = []
"1".upto("5") { |n| nums << n }
puts nums.inspect  # ["1", "2", "3", "4", "5"]

# Multi-char
words = []
"aa".upto("ad") { |w| words << w }
puts words.inspect  # ["aa", "ab", "ac", "ad"]

# Blockless returns array
puts "x".upto("z").inspect  # ["x", "y", "z"]

# Start == end
puts "a".upto("a").inspect  # ["a"]

# Start > end: empty
puts "z".upto("a").inspect  # []
