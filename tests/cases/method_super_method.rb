class Base
  def foo(x = nil)
    :base
  end
end

class A < Base
  def foo(x = nil)
    :a
  end
end

class B < A
  def foo(x = nil)
    :b
  end
end

m = B.new.method(:foo)
puts m.respond_to?(:super_method)
puts m.owner.name
puts m.super_method.respond_to?(:call)
puts m.super_method.owner.name

um = B.instance_method(:foo)
puts um.respond_to?(:super_method)
puts um.super_method.owner.name
puts um.super_method.super_method.respond_to?(:bind)
puts um.super_method.super_method.owner.name

puts Base.instance_method(:foo).super_method.inspect
puts B.new.method(:object_id).super_method.inspect
