a = [1, [2, [3, [4]]]]
p a.flatten
p a.flatten(1)
p a.flatten(2)
p a.flatten(0)

b = [[1, [2]], [3, [4, [5]]]]
p b.flatten(1)
