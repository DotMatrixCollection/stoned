x = 42
p "Value: #{x}"
p "Calc: #{x * 2 + 1}"
arr = [1,2,3]
mapped = arr.map { |n| n * 2 }
p "Method: #{mapped.join(", ")}"
p "Array: #{arr.inspect}"
