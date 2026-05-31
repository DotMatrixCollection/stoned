s = "hello world"
removed = s.slice!(0, 5)
puts removed.inspect
puts s.inspect

t = "abcdef"
ch = t.slice!(2)
puts ch.inspect
puts t.inspect

u = "test"
puts u.slice!(10, 3).inspect
puts u.inspect

# negative index
v = "hello"
puts v.slice!(-3, 2).inspect
puts v.inspect
