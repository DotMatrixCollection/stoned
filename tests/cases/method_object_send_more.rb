m = 42.method(:+)
p m.call(8)
p m.arity
p "hello".respond_to?(:upcase)
p "hello".respond_to?(:nonexistent)
p 42.send(:+, 8)
p "hello".send(:[], 1..3)
