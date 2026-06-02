# Integer#chr and String#ord
p 65.chr
p 97.chr
p 48.chr
p 10.chr

# ord goes the other way
p "A".ord
p "a".ord
p "0".ord
p "\n".ord

# chr with encoding
p 65.chr(Encoding::UTF_8)
p 233.chr(Encoding::UTF_8)  # é

# Round-trip
p "Z".ord.chr
p 90.chr.ord

# bytes/codepoints on strings
p "ABC".bytes.to_a
p "ABC".codepoints.to_a

# String#chars and #each_char
p "hello".chars
result = []
"hi".each_char { |c| result << c.ord }
p result

# Integer predicates related to char ranges
p 65.between?(65, 90)   # A-Z
p 97.between?(97, 122)  # a-z
p ("a".ord + 25).chr    # z
