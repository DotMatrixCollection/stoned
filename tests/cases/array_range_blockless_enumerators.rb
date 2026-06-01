a = [1, 2, 3]

array_methods = [
  :map,
  :collect,
  :select,
  :filter,
  :reject,
  :map!,
  :collect!,
  :select!,
  :filter!,
  :keep_if,
  :reject!,
  :delete_if
]

p array_methods.map { |m| a.dup.send(m).class }
p a.map.to_a
p a.select.to_a
p a.reject.to_a
p a.freeze.map!.class
p a.freeze.select!.to_a

r = 1..3
p [r.map.class, r.collect.class, r.select.class, r.filter.class, r.reject.class]
p r.map.to_a
p r.select.to_a
p r.reject.to_a
p ("a".."c").map.class
p ("a".."c").map.to_a

p [(1..3).any?, (1...1).any?, (1..3).all?, (1...1).none?]
