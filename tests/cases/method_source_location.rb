class MethodSourceLocationProbe
  def foo(x = nil)
    x
  end
end

m = MethodSourceLocationProbe.new.method(:foo)
puts m.respond_to?(:source_location)
puts m.source_location[0] == __FILE__
puts m.source_location[1]

um = MethodSourceLocationProbe.instance_method(:foo)
puts um.respond_to?(:source_location)
puts um.source_location[0] == __FILE__
puts um.source_location[1]

puts [1, 2].method(:each).respond_to?(:source_location)
puts [1, 2].method(:each).source_location.inspect
puts Array.instance_method(:each).source_location.inspect
