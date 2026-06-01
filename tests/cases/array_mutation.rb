arr = [1, 2]
arr.push(3)
arr.unshift(0)
puts arr.length
puts arr[0]
puts arr[3]
puts arr.respond_to?(:clear)
puts arr.respond_to?(:delete)
puts Array.instance_methods.include?(:clear)
puts Array.instance_methods.include?(:delete)
