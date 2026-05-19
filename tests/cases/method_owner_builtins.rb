puts [1, 2].method(:each).owner.name
puts [1, 2].method(:include?).owner.name
puts [1, 2].each.method(:with_index).owner.name
puts Array.instance_method(:each).owner.name

m = [1, 2].method(:each)
puts m.unbind.owner.name
puts Array.instance_method(:each).bind([3, 4]).owner.name
