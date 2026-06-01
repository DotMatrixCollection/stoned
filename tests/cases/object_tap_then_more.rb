p 1.then { |x| x + 1 }
p 1.yield_self { |x| x * 3 }
p 1.tap { |x| @seen = x }
