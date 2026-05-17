puts Encoding::UTF_8.class
puts Encoding::ASCII_8BIT.class
enc = Encoding.find("UTF_8")
puts enc.class
puts Encoding.find("bogus_encoding").class
puts Encoding.default_external.name
puts Encoding.default_internal.nil?
Encoding.default_external = "US_ASCII"
puts Encoding.default_external.name
Encoding.default_external = Encoding::UTF_8
puts Encoding.default_external.name
Encoding.default_internal = Encoding::ASCII_8BIT
puts Encoding.default_internal.name
