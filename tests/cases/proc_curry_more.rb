p = proc { |a, b| a + b }
c = p.curry
p c.call(2).call(3)
p p.curry(2).call(4).call(5)
