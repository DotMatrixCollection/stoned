puts Encoding::UTF_8.class
puts Encoding::ASCII_8BIT.class
enc = Encoding.find("UTF_8")
puts enc.class
puts Encoding.find("bogus_encoding").class
