b = binding

puts Binding.instance_methods(false).include?(:eval)
puts Binding.instance_methods(false).include?(:local_variable_get)
puts IO.instance_methods(false).include?(:read)
puts IO.instance_methods(false).include?(:close)
