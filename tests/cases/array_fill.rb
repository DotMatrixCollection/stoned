a = [1, 2, 3, 4]
p a.fill(:x)
p a

b = [1, 2, 3, 4]
p b.fill(:x, 1, 2)
p b

c = [1, 2, 3, 4]
p c.fill(:x, 1..2)
p c

d = [1, 2]
p d.fill(:z, 4, 2)
p d

e = [0, 0, 0]
p e.fill(1..2) { |i| i * 10 }
p e

p e.respond_to?(:fill)
p Array.instance_methods.include?(:fill)
