err = RuntimeError.new("boom")
p err.message
p err.exception.equal?(err)

copy = err.exception("other")
p copy.class
p copy.message

err.set_backtrace(["one.rb:1", "two.rb:2"])
p err.backtrace
