s = Struct.new(:a, :b).new(1, 2)

puts s.respond_to?(:each)
puts s.respond_to?(:members)
puts s.respond_to?(:to_h)
