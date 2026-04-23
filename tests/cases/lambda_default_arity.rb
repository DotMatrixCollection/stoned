l1 = ->(x = 1) { x }
puts l1.call
puts l1.call(5)
puts l1.arity

l2 = ->(x, y = 2) { [x, y].join(",") }
puts l2.call(1)
puts l2.call(1, 3)
puts l2.arity

l3 = ->(*xs) { xs.length }
puts l3.call
puts l3.call(1, 2)
puts l3.arity
