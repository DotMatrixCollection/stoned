def plus(a, b); a + b; end
m = method(:plus)
p m.to_proc.call(2, 3)
p m.curry.call(4).call(5)
