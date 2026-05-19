class Adder
  def add(a, b)
    a + b
  end
end

o = Object.new
m = Adder.new.method(:add)
um = Adder.instance_method(:add)

puts o.respond_to?(:clone)
puts o.respond_to?(:dup)
puts 1.respond_to?(:clone)
puts 1.respond_to?(:dup)
puts nil.respond_to?(:clone)
puts nil.respond_to?(:dup)
puts true.respond_to?(:clone)
puts true.respond_to?(:dup)
puts m.respond_to?(:clone)
puts m.respond_to?(:dup)
puts um.respond_to?(:clone)
puts um.respond_to?(:dup)
