$stdout.sync = true

# srand seeds the random number generator; rand is reproducible after seeding

srand(42)
r1 = rand(100)
srand(42)
r2 = rand(100)
puts r1 == r2   # true (same seed, same result)
puts r1.class   # Integer

srand(42)
puts rand.class  # Float

# rand(0) or rand with no int arg returns Float
puts rand(0).class  # Float (treating 0 as no arg)

# srand returns the old seed (or the given seed)
returned = srand(99)
puts returned.class  # Integer
