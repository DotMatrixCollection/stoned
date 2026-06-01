obj = Object.new
obj.instance_variable_set(:@x, 10)
p obj.instance_variable_get(:@x)
p obj.instance_variables
