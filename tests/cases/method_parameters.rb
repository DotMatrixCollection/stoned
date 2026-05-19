class MethodParametersProbe
  def foo(a, b = 1, *rest, &blk)
  end
end

m = MethodParametersProbe.new.method(:foo)
puts m.respond_to?(:parameters)
puts m.parameters.inspect

um = MethodParametersProbe.instance_method(:foo)
puts um.respond_to?(:parameters)
puts um.parameters.inspect

puts [1, 2].method(:each).parameters.inspect
puts Array.instance_method(:each).parameters.inspect
