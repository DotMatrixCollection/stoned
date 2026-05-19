md = /a(b)/.match("ab")
e = RuntimeError.new("x")

puts md.respond_to?(:captures)
puts md.respond_to?(:names)
puts e.respond_to?(:message)
puts e.respond_to?(:backtrace)
puts e.respond_to?(:set_backtrace)
