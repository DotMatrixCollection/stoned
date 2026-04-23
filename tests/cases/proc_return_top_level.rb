p = proc { return 1 }
puts p.class
p.call
puts "after"
