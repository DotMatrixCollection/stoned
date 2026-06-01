methods = [
  :casecmp,
  :casecmp?,
  :partition,
  :rpartition,
  :byteslice
]

p methods.map { |m| "abc".respond_to?(m) }
p methods.all? { |m| String.instance_methods.include?(m) }
p "abc".casecmp("ABC")
p "abc".casecmp?("ABC")
p "abc:def".partition(":")
p "abc:def:ghi".rpartition(":")
p "abc".byteslice(1, 2)
