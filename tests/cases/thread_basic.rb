t = Thread.new { 42 }
puts t.class
puts t.join.class
puts t.value
puts t.alive?
puts t.status

t2 = Thread.new { "hello" }
puts t2.value

Thread.current[:data] = "shared"
puts Thread.current[:data]
puts Thread.current.class
puts Thread.main.class
puts Thread.current.equal?(Thread.main)
