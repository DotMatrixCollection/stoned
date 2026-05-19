class Adder
  def add(a, b)
    a + b
  end
end

m = Adder.new.method(:add)
puts m.respond_to?(:curry)
c = m.curry
puts c.class
puts c.call(2).call(3)

include_curried = [1, 2, 3].method(:include?).curry
puts include_curried.call(2)
