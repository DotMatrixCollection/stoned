err = RuntimeError.new("boom")
same = err.exception
other = err.exception("later")

puts same.equal?(err)
puts err.methods.include?(:message)
puts err.methods.include?(:backtrace)
puts err.methods.include?(:set_backtrace)
puts other.class
puts other.message
puts other.backtrace.class
