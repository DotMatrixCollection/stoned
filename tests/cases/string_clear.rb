s = "hello"
s.clear
puts s.inspect
puts s.length
puts s.respond_to?(:clear)
puts String.instance_methods.include?(:clear)
