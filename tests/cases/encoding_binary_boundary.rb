path = "tests/fixtures/binary_sample.bin"
s = File.open(path, "rb") { |f| f.read }

puts s.bytesize
puts s.bytes.inspect
puts s.encoding.name
puts s.valid_encoding?
puts s.ascii_only?
puts s.respond_to?(:valid_encoding?)
puts s.respond_to?(:ascii_only?)
puts s.byteslice(0).bytes.inspect
puts s.byteslice(0).encoding.name
puts s.b.encoding.name
puts "abc".encoding.name
puts "abc".b.encoding.name
puts "abc".force_encoding("ASCII-8BIT").encoding.name
puts "abc".force_encoding(Encoding::UTF_8).encoding.name
