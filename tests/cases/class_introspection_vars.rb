class Foo
  MY_CONST = 42
  @@class_var = "cv"

  def initialize(x); @x = x; end
  def bar; @x; end
  private
  def secret; "shhh"; end
end

f = Foo.new(10)
p f.instance_variables
p Foo.instance_methods(false).sort
p Foo.private_instance_methods(false).sort
p Foo.const_defined?(:MY_CONST)
p Foo.const_get(:MY_CONST)
p Foo.class_variables
