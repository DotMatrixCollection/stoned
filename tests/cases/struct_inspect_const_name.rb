$stdout.sync = true

# Struct#inspect shows member names and values
# Struct class gets constant name when assigned

Point = Struct.new(:x, :y)
p = Point.new(3, 4)

puts Point.name    # Point (renamed from Struct::AnonymousN)
puts p.inspect     # #<struct Point x=3 y=4>
puts p.to_s        # #<struct Point x=3 y=4>

# Members and to_a still work
puts p.members.inspect  # [:x, :y]
puts p.to_a.inspect     # [3, 4]
puts p.to_h.inspect     # {:x=>3, :y=>4}

# Struct with block (user-defined to_s)
Box = Struct.new(:w, :h) do
  def area; w * h; end
  def to_s; "Box(#{w}x#{h})"; end
end
b = Box.new(3, 5)
puts b.area       # 15
puts b.to_s       # Box(3x5)
puts b.inspect    # #<struct Box w=3 h=5>

# Anonymous struct (no constant assignment)
Anon = Struct.new(:v)
a = Anon.new(99)
puts a.inspect   # #<struct Anon v=99>

# Struct equality
p2 = Point.new(3, 4)
p3 = Point.new(1, 2)
puts p == p2  # true
puts p == p3  # false
