r = Random.new(42)
x = r.rand(100)
puts x.is_a?(Integer)
puts x >= 0
puts x < 100

r2 = Random.new(42)
puts r2.rand(100) == r2.rand(100)  # same object: different successive calls

r3 = Random.new(42)
r4 = Random.new(42)
puts r3.rand(100) == r4.rand(100)  # same seed: same first value

f = Random.new(1).rand
puts f.is_a?(Float)
puts f >= 0.0
puts f < 1.0

puts Random.new(99).rand(10).is_a?(Integer)
