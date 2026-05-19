r, w = IO.pipe
w.write("hi")
w.close
puts r.read
r.close

puts r.closed?
puts w.closed?
