Point = Struct.new(:x, :y)
a = Point.new(1, 2)
b = Point.new(1, 2)
c = Point.new(1, 3)

# == uses value comparison
p a == b
p a == c

# eql? also uses value comparison
p a.eql?(b)
p a.eql?(c)

# Equal structs have equal hash codes
p a.hash == b.hash
p a.hash == c.hash

# respond_to? for these methods
p a.respond_to?(:eql?)
p a.respond_to?(:hash)

# Different class, same values => not equal
Other = Struct.new(:x, :y)
d = Other.new(1, 2)
p a == d
p a.eql?(d)
