class BoundBox
  def x; 10; end
end

um = BoundBox.instance_method(:x)
obj = BoundBox.new
p um.bind(obj).call
p um.bind_call(obj)
