class Calculator
  def add(a, b); a + b; end
  def multiply(a, b); a * b; end
end

calc = Calculator.new
m = calc.method(:add)
p m.call(3, 4)
p m.arity
p m.class
p m.name
p m.owner

unbound = Calculator.instance_method(:multiply)
p unbound.class
bound = unbound.bind(calc)
p bound.call(5, 6)
