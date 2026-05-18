$stdout.sync = true

# String#split with no arg uses Ruby whitespace-split semantics:
# strip leading/trailing, collapse whitespace runs, no empty fields

puts "  hello  world  ".split.inspect       # ["hello", "world"]
puts "\thello\n world\r\n".split.inspect    # ["hello", "world"]
puts "one two three".split.inspect          # ["one", "two", "three"]
puts "  ".split.inspect                     # []
puts "".split.inspect                       # []

# nil arg is same as no arg
puts "  a  b  ".split(nil).inspect          # ["a", "b"]

# " " (space string) is also whitespace split
puts "  x  y  ".split(" ").inspect          # ["x", "y"]

# Normal separator split still works
puts "a,b,,c".split(",").inspect            # ["a","b","","c"]
puts "abc".split("").inspect                # ["a","b","c"]

# Split with limit
puts "a b c d".split(" ", 2).inspect        # ["a","b c d"] — limit applies
