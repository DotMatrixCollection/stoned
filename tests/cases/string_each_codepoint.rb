result = []
"hello".each_codepoint { |cp| result << cp }
p result

p "hello".codepoints
p "ABC".codepoints

result2 = []
"hi!".each_char { |c| result2 << [c, c.ord] }
p result2

p "abc".chars.zip("abc".codepoints)
