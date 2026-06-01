array_methods = [
  :intersection,
  :union,
  :difference,
  :repeated_combination,
  :repeated_permutation,
  :rotate,
  :rotate!,
  :delete_at,
  :insert,
  :flatten!,
  :compact!
]

hash_methods = [
  :compact,
  :compact!,
  :fetch_values,
  :slice,
  :except
]

p array_methods.map { |m| [].respond_to?(m) }
p array_methods.all? { |m| Array.instance_methods.include?(m) }
p hash_methods.map { |m| {}.respond_to?(m) }
p hash_methods.all? { |m| Hash.instance_methods.include?(m) }

p [1, 2, 3].intersection([2, 4])
p [1, 2].union([2, 3])
p [1, 2, 3].difference([2])
p({a: 1, b: nil}.compact)
p({a: 1, b: 2}.fetch_values(:b, :a))
