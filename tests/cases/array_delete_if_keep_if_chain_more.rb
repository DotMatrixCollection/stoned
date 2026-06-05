a = [1, 2, 3, 4, 5]
p a.delete_if { |n| n.odd? }
p a
b = [1, 2, 3, 4]
p b.keep_if { |n| n > 2 }
