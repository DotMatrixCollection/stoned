# String#split with various separators and limits
p "a,b,c,d".split(",")
p "a,b,c,d".split(",", 2)
p "a,b,c,d".split(",", -1)
p "a,,b,,c".split(",")
p "a,,b,,c".split(",", -1)

# Regex separator
p "one1two2three3four".split(/\d/)
p "hello   world  foo".split(/\s+/)
p "hello   world  foo".split(/\s+/, 2)

# No argument splits on whitespace and strips leading
p "  hello  world  ".split
p " a  b  c ".split(" ")

# Split with capture group includes separators
p "one,two;three".split(/([,;])/)

# Empty string splits into chars
p "abc".split("")
p "abc".split("", 2)

# Trailing empty strings
p "a,b,".split(",")
p "a,b,".split(",", -1)

# Single char
p "hello".split("l")
