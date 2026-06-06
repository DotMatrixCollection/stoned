a = ["zero", "one", "two", "three", "four"]
p a.values_at(0, 2, 4)
p a.values_at(1, 3)
result = []
a.values_at(0, 1, 2).cycle(2) { |x| result << x }
p result
