err = RuntimeError.new("boom")
same = err.exception
other = err.exception("later")

puts same.equal?(err)
puts other.class
puts other.message
puts other.backtrace.class
