puts Proc.class
puts Proc.instance_of?(Class)
puts Proc.is_a?(Module)
puts Proc.is_a?(Class)

puts Module.class
puts Module.instance_of?(Class)
puts Module.is_a?(Module)
puts Module.is_a?(Class)

puts Class.class
puts Class.instance_of?(Class)
puts Class.is_a?(Module)
puts Class.is_a?(Class)

puts Proc.respond_to?(:new)
puts Proc.respond_to?(:respond_to_missing?)
puts Proc.respond_to?(:class)
puts Module.respond_to?(:new)
puts Class.respond_to?(:new)
