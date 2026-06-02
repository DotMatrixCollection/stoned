pad = proc { |a, b, c| [a, b, c] }
rest = proc { |a, *tail| [a, tail] }
strict = lambda { |a, b = 7, *tail| [a, b, tail] }

p pad.call(1)
p pad.call(1, 2, 3, 4)
p rest.call(1, 2, 3)
p strict.call(1)
p strict.call(1, 2, 3, 4)

begin
  strict.call
rescue ArgumentError => e
  p e.class
  p e.message
end
