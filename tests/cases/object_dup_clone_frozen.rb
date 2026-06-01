str = "hello"
str.freeze
p str.frozen?
dup_str = str.dup
p dup_str.frozen?
dup_str << " world"
p dup_str

clone_str = str.clone
p clone_str.frozen?

arr = [1, 2, 3].freeze
p arr.frozen?
arr2 = arr.dup
p arr2.frozen?
arr2 << 4
p arr2

class MyObj
  attr_accessor :x
  def initialize(x); @x = x; end
end

obj = MyObj.new(42)
obj2 = obj.dup
obj2.x = 99
p obj.x
p obj2.x
