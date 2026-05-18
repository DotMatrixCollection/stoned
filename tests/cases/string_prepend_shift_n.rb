$stdout.sync = true

# String#prepend — in-place prepend, mutation propagation
s = "world"
s.prepend("hello ")
puts s   # hello world

# Prepend multiple args
s2 = "c"
s2.prepend("a", "b")
puts s2  # abc

# Array#shift(n) — return first n elements as array
a = [1, 2, 3, 4, 5]
puts a.shift(2).inspect   # [1, 2]
puts a.inspect             # [3, 4, 5]
puts a.shift(0).inspect   # []
puts a.inspect             # [3, 4, 5]
puts a.shift(10).inspect  # [3, 4, 5] (shifts all, no error)
puts a.inspect             # []
puts a.shift(1).inspect   # [] (empty array)

# Array#shift() — no-arg still works
b = [10, 20, 30]
puts b.shift   # 10
puts b.inspect # [20, 30]
