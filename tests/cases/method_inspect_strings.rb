class T
  def foo(x = nil)
  end
end

obj = T.new
puts obj.method(:foo).to_s
puts obj.method(:foo).inspect
puts T.instance_method(:foo).to_s
puts T.instance_method(:foo).inspect
puts [1, 2].method(:each).to_s
puts Array.instance_method(:each).to_s
