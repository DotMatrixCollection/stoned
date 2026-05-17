opts = Object.new
def opts.alias(a,b) = a.to_s+b.to_s
puts opts.alias("x","y")
x = 42
puts x.class
