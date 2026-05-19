class Adder
  def add(a, b)
    a + b
  end
end

m = Adder.new.method(:add)
p = m.to_proc

puts p.class
puts p.arity
puts p.lambda?
puts p.curry.call(2).call(3)

include_proc = [1, 2, 3].method(:include?).to_proc
puts include_proc.arity
