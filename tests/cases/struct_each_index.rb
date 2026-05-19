Point = Struct.new(:x, :y)
p = Point.new(3, 4)

# each / each_pair
vals = []
p.each { |v| vals << v }
p vals

pairs = {}
p.each_pair { |k, v| pairs[k] = v }
p pairs

# [] by index and symbol
p p[0]
p p[1]
p p[-1]
p p[:x]
p p[:y]

# []= mutation
p[:x] = 10
p p.x
p p[0]

# Enumerable via included module
p p.map { |v| v * 2 }
p p.select { |v| v > 5 }
p p.min
p p.sum
p p.include?(10)

# break from each
p p.each { |v| break v * 3 if v == 4 }
