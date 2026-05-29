p nil.to_i
p nil.to_f
p nil.to_r.inspect
p nil.to_r.class
p nil.to_c.inspect
p nil.to_c.class
p nil.to_a
p nil.to_h
p nil.to_s
p nil.nil?
p nil.inspect
p [1, 2, 3, 2].delete(2)
p [1, 2, 3].delete(99)
p [1, 2, 3].delete(99) { "not found" }
p [1, 2, 3, 4, 5].pop(3)
a = [1, 2, 3]
a.clear
p a
