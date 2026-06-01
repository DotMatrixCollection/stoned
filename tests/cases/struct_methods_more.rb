Point = Struct.new(:x, :y)
p1 = Point.new(1, 2)
p p1.to_a
p p1.to_h
p p1.members
p p1.x
p p1[:y]
p p1 == Point.new(1, 2)
p p1 == Point.new(1, 3)

Person = Struct.new(:name, :age) do
  def greeting
    "Hi, I'm #{name}, #{age} years old"
  end
end

p Person.new("Alice", 30).greeting
