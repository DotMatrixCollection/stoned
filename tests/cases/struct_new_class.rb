# Struct.new creates value-like classes
Point = Struct.new(:x, :y)
p1 = Point.new(1, 2)
p p1.x
p p1.y
p p1.members
p p1.to_a
p p1.to_h

# Struct equality
p2 = Point.new(1, 2)
p3 = Point.new(3, 4)
p p1 == p2
p p1 == p3

# Struct with block adds methods
Circle = Struct.new(:center, :radius) do
  def area = Math::PI * radius ** 2
  def to_s = "Circle@#{center} r=#{radius}"
end

c = Circle.new(Point.new(0, 0), 5)
p c.radius
p c.area.round(4)
p c.to_s

# Struct members are mutable
p1.x = 10
p p1.x
p p1 == p2

# keyword_init: true (Ruby 2.5+)
Person = Struct.new(:name, :age, keyword_init: true)
bob = Person.new(name: "Bob", age: 30)
p bob.name
p bob.age
p bob.to_h

# Struct#each and #each_pair
results = []
p2.each { |v| results << v }
p results

pairs = {}
p2.each_pair { |k, v| pairs[k] = v }
p pairs
