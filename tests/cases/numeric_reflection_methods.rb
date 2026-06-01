integer_methods = [
  :to_int,
  :abs2,
  :<<,
  :>>,
  :&,
  :|,
  :^,
  :~
]

float_methods = [
  :abs2,
  :integer?,
  :nonzero?,
  :step
]

p integer_methods.map { |m| 42.respond_to?(m) }
p integer_methods.all? { |m| Integer.instance_methods.include?(m) }
p float_methods.map { |m| 1.5.respond_to?(m) }
p float_methods.all? { |m| Float.instance_methods.include?(m) }
