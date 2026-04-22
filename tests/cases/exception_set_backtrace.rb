err = RuntimeError.new("boom")
err.set_backtrace(["a", "b"])
puts err.backtrace.join(" | ")
err.set_backtrace(nil)
puts err.backtrace.nil?
