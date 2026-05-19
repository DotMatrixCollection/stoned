class A
  def foo
    :a
  end
end

class B
end

class C < A
end

um = A.instance_method(:foo)

begin
  um.bind(B.new)
rescue => e
  puts e.class
  puts e.message.include?("A")
end

puts um.bind(C.new).owner.name

begin
  um.bind_call(B.new)
rescue => e
  puts e.class
  puts e.message.include?("foo")
end
