a = [3, 1, 2, 1, 3]
a.sort!
p a
a.uniq!
p a

b = [1, nil, 2, nil, 3]
b.compact!
p b

c = [1, [2, [3]]]
c.flatten!
p c

d = [2, 1, 3]
p d.sort!
p d.sort! { |a, b| b <=> a }
