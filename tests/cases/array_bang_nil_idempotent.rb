a = [1, 2, 2, 3]
p a.uniq!
p a.uniq!

b = [1, nil, 2]
p b.compact!
p b.compact!

c = [1, [2, 3]]
p c.flatten!
p c.flatten!

d = [1, 2, 3, 4]
p d.select! { |x| x.even? }
p d.select! { |x| x.even? }
