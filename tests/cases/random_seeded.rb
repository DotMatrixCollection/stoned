r = Random.new(123)
a = [r.rand(10), r.rand(10), r.rand(10)]
r2 = Random.new(123)
b = [r2.rand(10), r2.rand(10), r2.rand(10)]
p a
p a == b
p Random::DEFAULT.is_a?(Random)
