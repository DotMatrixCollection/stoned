# Block parameters do not leak into the enclosing scope (Ruby 1.9+ semantics)
a = 10
[1, 2, 3].each { |a| a * 2 }
puts a          # 10 — outer a unchanged

b = "outer"
["x", "y"].map { |b| b.upcase }
puts b          # outer

# Closures can still read and write outer variables (not as params)
n = 0
inc = proc { n += 1 }
inc.call; inc.call
puts n          # 2
