# Array mutation: unshift, concat, append, prepend
arr = [3, 4, 5]
arr.unshift(1, 2)
p arr

arr2 = [1, 2, 3]
arr2.concat([4, 5], [6, 7])
p arr2

# append is alias for <<, push
arr3 = [1, 2]
arr3.append(3, 4)
p arr3
arr3 << 5
p arr3

# prepend is alias for unshift
arr4 = [3, 4]
arr4.prepend(1, 2)
p arr4

# += creates new array
a = [1, 2]
b = a
a += [3, 4]
p a
p b           # b unchanged; += is not mutation

# concat mutates
c = [1, 2]
d = c
c.concat([3, 4])
p c
p d.equal?(c)  # same object

# flatten!
nested = [1, [2, [3, 4]], 5]
nested.flatten!
p nested

# uniq!
dupes = [1, 2, 2, 3, 3, 3]
dupes.uniq!
p dupes

# compact!
with_nils = [1, nil, 2, nil, 3]
with_nils.compact!
p with_nils
