methods = [
  :casecmp,
  :casecmp?,
  :partition,
  :rpartition,
  :byteslice,
  :concat,
  :prepend,
  :slice!
]

p methods.map { |m| "abc".respond_to?(m) }
p methods.all? { |m| String.instance_methods.include?(m) }
p "abc".casecmp("ABC")
p "abc".casecmp?("ABC")
p "abc:def".partition(":")
p "abc:def:ghi".rpartition(":")
p "abc".byteslice(1, 2)
s = "a"
p s.concat("b")
p s.prepend("z")
p s.slice!(1, 1)
p s
