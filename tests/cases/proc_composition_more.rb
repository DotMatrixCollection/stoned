add = ->(x) { x + 1 }
dbl = ->(x) { x * 2 }
p (add >> dbl).call(3)
p (add << dbl).call(3)
