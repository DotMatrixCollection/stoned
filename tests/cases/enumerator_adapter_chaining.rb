e1 = [1, 2].each_with_object([0])
puts e1.class
puts e1.to_a.inspect

e2 = {a: 1, b: 2}.each_with_object(:memo)
puts e2.class
puts e2.to_a.inspect

e3 = [10, 20].each.with_index(5)
puts e3.class
puts e3.to_a.inspect

e4 = [10, 20].each.with_object(:memo)
puts e4.class
puts e4.to_a.inspect
