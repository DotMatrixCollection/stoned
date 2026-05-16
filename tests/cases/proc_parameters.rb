f = proc {|x, y, z: 1| }
puts f.parameters.class
puts f.parameters.size

g = proc {|v| v }
puts g.parameters.first.first.inspect
