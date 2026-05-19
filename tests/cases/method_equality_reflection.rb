class MethodEqualityProbe
  def foo
    1
  end
end

arr = [1, 2]
other = [3, 4]
m1 = arr.method(:each)
m2 = arr.method(:each)
m3 = other.method(:each)
puts m1 == m2
puts m1 == m3
puts m1.eql?(m2)
puts m1.hash == m2.hash

u1 = Array.instance_method(:each)
u2 = Array.instance_method(:each)
u3 = Array.instance_method(:include?)
puts u1 == u2
puts u1 == u3
puts u1.eql?(u2)
puts u1.hash == u2.hash

obj = Object.new
puts obj.respond_to?(:hash)
puts obj.respond_to?(:eql?)
puts obj.eql?(obj)

probe = MethodEqualityProbe.new
puts probe.method(:foo) == probe.method(:foo)
puts MethodEqualityProbe.instance_method(:foo) == MethodEqualityProbe.instance_method(:foo)
