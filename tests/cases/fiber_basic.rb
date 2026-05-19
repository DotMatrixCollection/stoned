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
