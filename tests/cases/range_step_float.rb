$stdout.sync = true

# Range#step with float ranges

# Basic float step
steps = []
(0.0..1.0).step(0.25) { |x| steps << x.round(10) }
puts steps.inspect   # [0.0, 0.25, 0.5, 0.75, 1.0]

# Blockless returns array
puts (0.0..0.5).step(0.1).map { |x| x.round(1) }.inspect

# Mixed int range with float step
puts (1..3).step(0.5).to_a.map { |x| x.round(1) }.inspect  # [1.0, 1.5, 2.0, 2.5, 3.0]

# Integer step still works
puts (1..10).step(3).to_a.inspect   # [1, 4, 7, 10]

# Exclusive range
result = []
(0.0...1.0).step(0.5) { |x| result << x.round(10) }
puts result.inspect   # [0.0, 0.5] (1.0 excluded)
