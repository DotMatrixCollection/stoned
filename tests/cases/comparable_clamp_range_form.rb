# Integer clamp with single Range argument (Ruby 2.7+)
p 5.clamp(1..10)
p 0.clamp(1..10)
p 15.clamp(1..10)

# Beginless/endless ranges
p 5.clamp(..10)
p 5.clamp(1..)
p 15.clamp(..10)
p 0.clamp(1..)

# Float clamp with range
p 3.14.clamp(0.0..5.0)
p 0.5.clamp(1.0..5.0)
p 6.0.clamp(1.0..5.0)

# Exclusive range clamps to just below endpoint for integers
p 5.clamp(1...10)
