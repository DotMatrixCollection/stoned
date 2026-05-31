class Foo
  def initialize(x = 0)
    @x = x
  end
  def x; @x; end
end

obj = Foo.allocate
puts obj.class
puts obj.x.inspect

obj.instance_variable_set(:@x, 42)
puts obj.x

obj2 = Foo.new(99)
puts obj2.x
puts obj2.class
