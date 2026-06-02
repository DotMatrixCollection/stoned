# __method__ returns current method name as symbol
def my_method
  __method__
end
p my_method

def another
  __method__
end
p another

# __method__ at top level returns nil
p __method__

# __callee__ returns the called name (including aliased names)
class Foo
  def original
    __callee__
  end
  alias aliased original
end

foo = Foo.new
p foo.original
p foo.aliased

# __method__ inside block still reflects enclosing method
def method_with_block
  [1, 2, 3].map { __method__ }
end
p method_with_block

# __method__ in define_method
klass = Class.new
klass.define_method(:dynamic_name) { __method__ }
p klass.new.dynamic_name
