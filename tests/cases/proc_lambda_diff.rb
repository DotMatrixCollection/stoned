# Procs and lambdas differ in return behavior and arity checking
lam = lambda { |x| x * 2 }
prc = Proc.new { |x| (x || 0) * 2 }

p lam.call(5)
p prc.call(5)
p lam.lambda?
p prc.lambda?

# Lambda enforces argument count; proc is lenient
p lam.arity
p prc.arity

# Proc accepts wrong arity silently
p prc.call(3, 4, 5)  # extra args ignored
p prc.call           # missing args become nil

# Stabby lambda
double = ->(x) { x * 2 }
p double.call(7)
p double.lambda?

# Lambda with multiple args
add = ->(a, b) { a + b }
p add.(3, 4)
p add.arity

# Proc created with proc {}
p2 = proc { |a, b| [a, b] }
p p2.lambda?
p p2.call(1)       # missing arg becomes nil
p p2.call(1, 2, 3) # extra arg discarded

# curry works on both
curried = add.curry
p curried.(10).(20)
