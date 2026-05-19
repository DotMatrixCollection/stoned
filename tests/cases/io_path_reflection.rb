f = File.open("/tmp/stoned_path_reflection.txt", "w")

puts IO.instance_methods(false).include?(:path)
puts f.respond_to?(:path)
puts f.path
puts binding.respond_to?(:eval)
puts binding.respond_to?(:local_variable_get)

f.close
