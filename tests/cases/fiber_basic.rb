# Basic fiber resume/yield cycle
f = Fiber.new do |x|
  y = Fiber.yield x * 2
  y + 100
end

puts f.alive?          # true
puts f.resume(5)       # 10
puts f.alive?          # true
puts f.resume(7)       # 107
puts f.alive?          # false

# FiberError on dead fiber
begin
  f.resume
rescue FiberError => e
  puts e.class         # FiberError
  puts e.message
end

# Fiber with no args
g = Fiber.new { 42 }
puts g.resume          # 42
puts g.alive?          # false

# Fiber as generator
fib = Fiber.new do
  a, b = 0, 1
  loop do
    Fiber.yield a
    a, b = b, a + b
  end
end
6.times { print "#{fib.resume} " }
puts

# Fiber.current outside = nil
puts Fiber.current.nil?  # true

# Fiber.current inside = the fiber
inner = Fiber.new { Fiber.current.class }
puts inner.resume   # Fiber

# FiberError is subclass of StandardError
puts FiberError.ancestors.include?(StandardError)  # true

# Multiple resume/yield values follow MRI packing rules
multi = Fiber.new do |a, b|
  p [:start, a, b]
  incoming = Fiber.yield(:y1, :y2)
  p [:incoming, incoming]
  Fiber.yield(:single)
end

p multi.resume(1, 2)
p multi.resume(:r1, :r2)
p multi.resume

begin
  Fiber.yield
rescue FiberError => e
  puts e.message
end

begin
  Fiber.new
rescue ArgumentError => e
  puts e.message
end
