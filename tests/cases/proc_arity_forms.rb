p proc {}.arity
p proc { |a| }.arity
p proc { |a, b| }.arity
p proc { |*a| }.arity
p proc { |a, *b| }.arity
p proc { |a, b, *c| }.arity
p proc { |a, b, c| }.arity

p lambda {}.arity
p lambda { |a| }.arity
p lambda { |a, b| }.arity
p lambda { |*a| }.arity
p lambda { |a, *b| }.arity
