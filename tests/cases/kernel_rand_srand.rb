srand(42)
r1 = rand
r2 = rand
p r1.is_a?(Float)
p r2.is_a?(Float)
p (r1 >= 0 && r1 < 1)

srand(42)
p rand == r1

p rand(10).is_a?(Integer)
p rand(10) >= 0
p rand(1..6).is_a?(Integer)
