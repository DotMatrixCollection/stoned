class Point3D < Struct.new(:x, :y, :z)
  def magnitude
    Math.sqrt(x**2 + y**2 + z**2)
  end
end

pt = Point3D.new(1, 2, 2)
puts pt.x
puts pt.y
puts pt.z
puts pt.magnitude

puts Point3D.members.inspect
puts pt.to_a.inspect

# Equality from Struct works in subclasses
pt2 = Point3D.new(1, 2, 2)
pt3 = Point3D.new(0, 0, 0)
puts (pt == pt2)
puts (pt == pt3)
