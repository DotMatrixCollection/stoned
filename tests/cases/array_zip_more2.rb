a = [1, 2, 3]
b = ["a", "b", "c"]
c = [:x, :y, :z]
p a.zip(b)
p a.zip(b, c)
p [1, 2].zip([3, 4, 5])

result = []
a.zip(b) { |pair| result << pair.inspect }
p result
