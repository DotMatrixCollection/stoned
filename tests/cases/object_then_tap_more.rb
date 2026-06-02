p [1,2,3].then { |a| a.sum }
p "hello".then { |s| s.upcase }
p nil.then { |x| x.to_s + "!" }
p 5.yield_self { |n| n * n }
p [1,2,3].tap { |a| a << 4 }.length
