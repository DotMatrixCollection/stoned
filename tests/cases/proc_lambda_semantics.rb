my_proc = proc { |x, y| [x, y] }
my_lambda = lambda { |x, y| [x, y] }

p my_proc.call(1, 2)
p my_lambda.call(1, 2)

p my_proc.lambda?
p my_lambda.lambda?

p my_proc.call(1)
p my_proc.call(1, 2, 3)

begin
  my_lambda.call(1)
rescue ArgumentError => e
  p "arity error: #{e.message}"
end
