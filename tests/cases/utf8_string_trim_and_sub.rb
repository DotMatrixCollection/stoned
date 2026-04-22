nbsp = " "
emsp = " "

puts "#{nbsp}hi#{emsp}".strip.inspect
puts "#{nbsp}hi".lstrip.inspect
puts "hi#{emsp}".rstrip.inspect

puts "héhé".scan("hé").inspect
puts "héhé".sub("hé", "ha")
puts "héhé".gsub("hé", "ha")
