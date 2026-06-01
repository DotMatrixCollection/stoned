b = binding

puts Binding.instance_methods(false).include?(:eval)
puts Binding.instance_methods(false).include?(:local_variable_get)
puts IO.instance_methods(false).include?(:read)
puts IO.instance_methods(false).include?(:close)
puts String.instance_methods.include?(:object_id)
puts Array.instance_methods.include?(:respond_to?)
puts Integer.instance_methods.include?(:itself)
puts Proc.instance_methods.include?(:method)
puts String.instance_methods(false).include?(:object_id)
