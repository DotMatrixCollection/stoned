p "hello".frozen?
s = "hello".freeze
p s.frozen?
a = [1, 2, 3].freeze
p a.frozen?
h = {a: 1}.freeze
p h.frozen?
p 42.frozen?
p :sym.frozen?
p nil.frozen?
p true.frozen?
p 1.eql?(1)
p 1.eql?(1.0)
p "hi".eql?("hi")
p "hi".eql?(42)
