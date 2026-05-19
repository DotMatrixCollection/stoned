puts Range.instance_methods(false).include?(:each)
puts Range.method_defined?(:each)
puts Range.public_method_defined?(:each)
puts Range.private_method_defined?(:each)

puts Symbol.instance_methods(false).include?(:to_proc)
puts Symbol.method_defined?(:to_proc)
puts Symbol.public_method_defined?(:to_proc)
puts Symbol.private_method_defined?(:to_proc)

puts Method.instance_methods(false).include?(:call)
puts Method.method_defined?(:call)
puts Method.public_method_defined?(:call)
puts Method.private_method_defined?(:call)

puts UnboundMethod.instance_methods(false).include?(:bind)
puts UnboundMethod.method_defined?(:bind)
puts UnboundMethod.public_method_defined?(:bind)
puts UnboundMethod.private_method_defined?(:bind)

puts IO.instance_methods(false).include?(:read)
puts IO.method_defined?(:read)
puts IO.public_method_defined?(:read)
puts IO.private_method_defined?(:read)
