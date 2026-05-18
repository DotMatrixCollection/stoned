$stdout.sync = true

# at_exit handlers run LIFO at program exit

at_exit { puts "first" }
at_exit { puts "second" }
at_exit { puts "third" }

puts "main"

# Expected output:
# main
# third   (last registered, runs first)
# second
# first   (first registered, runs last)
