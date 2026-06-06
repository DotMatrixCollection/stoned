# Data.define creates immutable value objects (Ruby 3.2+)
Point = Data.define(:x, :y)

p = Point.new({x: 1, y: 2})
p p.x
p p.y
p p.class

# Equality based on values
p1 = Point.new({x: 3, y: 4})
p2 = Point.new({x: 3, y: 4})
p3 = Point.new({x: 0, y: 0})
p p1 == p2
p p1 == p3

# Data objects are frozen
p p.frozen?

# Members list
p Point.members

# to_h
p p1.to_h

# with creates a copy with changed fields
p4 = p1.with({x: 10})
p p4.x
p p4.y

# Deconstruct and deconstruct_keys
p p1.deconstruct
p p1.deconstruct_keys([:x])
p p1.deconstruct_keys(nil)
