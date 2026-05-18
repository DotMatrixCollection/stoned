$stdout.sync = true

# String bang methods — in-place mutation and nil-on-no-change

# upcase!
s = "hello"
result = s.upcase!
puts result   # HELLO WORLD
puts s        # HELLO (mutated)

# Returns nil when no change
puts "HELLO".upcase!.nil?  # true

# downcase!
s2 = "WORLD"
s2.downcase!
puts s2  # world

# strip!
s3 = "  hi  "
s3.strip!
puts s3  # hi

# chomp!
s4 = "hello\n"
s4.chomp!
puts s4  # hello
puts "noeol".chomp!.nil?  # true

# gsub!
s5 = "hello"
s5.gsub!("l", "r")
puts s5  # herro

# Returns nil when no match
puts "hello".gsub!("z", "x").nil?  # true

# sub!
s6 = "aababc"
s6.sub!("a", "X")
puts s6  # Xababc

# capitalize!
s7 = "hello world"
s7.capitalize!
puts s7  # Hello world

# reverse!
s8 = "abcde"
s8.reverse!
puts s8  # edcba
