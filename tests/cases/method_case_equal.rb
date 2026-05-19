class Adder
  def add(a, b)
    a + b
  end
end

m = Adder.new.method(:add)
puts m.respond_to?(:===)
puts m.===(2, 3)
puts [1, 2, 3].method(:include?).===(2)
