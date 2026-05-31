# define_method with closure captures and mutates outer variable
class Counter
  count = 0
  define_method(:tick) { count += 1; count }
  define_method(:reset) { count = 0 }
end

c = Counter.new
puts c.tick   # 1
puts c.tick   # 2
puts c.tick   # 3
c.reset
puts c.tick   # 1

# All instances of the same class share the captured variable
c2 = Counter.new
puts c2.tick  # 2 (same count)
