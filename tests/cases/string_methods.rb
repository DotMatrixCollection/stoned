# chomp / chop / lstrip / rstrip
puts "hello\n".chomp
puts "hello\r\n".chomp
puts "hello\r".chomp
puts "hello".chomp
puts "hello\n".chop
puts "hello\r\n".chop
puts "hello".chop
puts "  hi  ".lstrip.inspect
puts "  hi  ".rstrip.inspect

# capitalize / swapcase
puts "hello WORLD".capitalize
puts "hello WORLD".swapcase

# ljust / rjust / center
puts "hi".ljust(6)        + "|"
puts "hi".ljust(6, "-")   + "|"
puts "hi".rjust(6)        + "|"
puts "hi".rjust(6, "0")   + "|"
puts "hi".center(6)       + "|"
puts "hi".center(7, "-")  + "|"
puts "hi".ljust(1)        + "|"

# ord / hex / oct / bytes
puts "A".ord
puts "ff".hex
puts "0xff".hex
puts "0777".oct
puts "abc".bytes.inspect

# <<
a = "hello"
b = a << " world"
puts b

# index / rindex
puts "hello".index("l")
puts "hello".index("l", 4)
puts "hello".rindex("l")
puts "hello".rindex("x").inspect

# [] / slice
puts "hello"[1]
puts "hello"[-1]
puts "hello"[1, 3]
puts "hello"[10].inspect

# lines / each_line
puts "a\nb\nc\n".lines.length
"a\nb".each_line { |l| print l.chomp + "!" }
puts

# tr
puts "hello".tr("aeiou", "*")
puts "hello".tr("a-y", "b-z")
puts "hello".tr("el", "ip")

# count / delete / squeeze
puts "hello world".count("lo")
puts "hello".delete("l")
puts "aaabbbccc".squeeze
puts "aaabbbccc".squeeze("a-b")

# scan
puts "hello world".scan("l").inspect
puts "hello world".scan("o").length
"abcabc".scan("a") { |m| print m }
puts

# sub / gsub (string replacement)
puts "hello world".sub("world", "ruby")
puts "hello hello".gsub("hello", "hi")

# sub / gsub (block)
puts "hello".sub("l") { |m| m.upcase }
puts "hello".gsub("l") { |m| m.upcase }

# inspect
puts "hello\nworld".inspect
puts "say \"hi\"".inspect
