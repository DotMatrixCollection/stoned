plain = proc { |a, b = 1, *rest| [a, b, rest] }
strict = lambda { |a, b = 1, *rest| [a, b, rest] }

p plain.arity
p strict.arity
p plain.parameters
p strict.parameters
p plain.call(10)
p strict.call(10, 20, 30)
