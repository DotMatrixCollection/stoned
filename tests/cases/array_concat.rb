a = [1, 2, 3]
b = [4, 5]
a.concat(b)
puts a.inspect
a.concat([6], [7, 8])
puts a.inspect
puts a.respond_to?(:concat)
puts Array.instance_methods.include?(:concat)
