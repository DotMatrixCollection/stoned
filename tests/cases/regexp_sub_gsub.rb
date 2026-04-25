re = Regexp.new("[aeiou]")
puts "hello world".sub(re, "*")
puts "hello world".gsub(re, "*")
puts "hello world".sub(re) { |m| m.upcase }
puts "hello world".gsub(re) { |m| m.upcase }
puts "no match here".gsub(Regexp.new("x"), "Y")
puts "abc".sub(Regexp.new("b"), "B")
puts "aabbcc".gsub(Regexp.new("b+"), "-")
puts "hello".sub(Regexp.new("xyz"), "?")
