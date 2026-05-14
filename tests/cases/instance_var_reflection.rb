class Point
  def initialize(x, y)
    @x = x
    @y = y
  end
end

p = Point.new(1, 2)
p p.instance_variable_get(:@x)
p p.instance_variable_get("@y")
p p.instance_variable_set(:@z, 99)
p p.instance_variable_get(:@z)
p p.instance_variable_defined?(:@x)
p p.instance_variable_defined?(:@w)
p p.instance_variables.sort
