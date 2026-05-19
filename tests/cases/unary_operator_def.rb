# def -@ and def +@ for user classes
class Temperature
  attr_reader :degrees
  def initialize(d); @degrees = d; end
  def -@; Temperature.new(-@degrees); end
  def +@; self; end
  def -(other); Temperature.new(@degrees - other.degrees); end
  def +(other); Temperature.new(@degrees + other.degrees); end
  def to_s; "#{@degrees}°"; end
end

t = Temperature.new(20)
neg = -t
puts neg.degrees      # -20
pos = +t
puts pos.degrees      # 20

diff = t - Temperature.new(5)
puts diff.degrees     # 15

sum = t + Temperature.new(3)
puts sum.degrees      # 23

# def / operator
class Vector2
  attr_reader :x, :y
  def initialize(x, y); @x = x; @y = y; end
  def /(scalar); Vector2.new(@x.to_f / scalar, @y.to_f / scalar); end
  def to_s; "(#{@x}, #{@y})"; end
end

v = Vector2.new(10, 20)
half = v / 2
puts half.x    # 5.0
puts half.y    # 10.0
