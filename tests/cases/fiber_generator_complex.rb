# Counter fiber as infinite stream
counter = Fiber.new do
  n = 0
  loop { Fiber.yield n; n += 1 }
end
results = 5.times.map { counter.resume }
p results

# Fibonacci generator
fib = Fiber.new do
  a, b = 0, 1
  loop do
    Fiber.yield a
    a, b = b, a + b
  end
end
p 8.times.map { fib.resume }

# Fiber passing values back in on resume
accumulator = Fiber.new do
  total = 0
  loop do
    n = Fiber.yield total
    total += n
  end
end
accumulator.resume   # start it
accumulator.resume(10)
accumulator.resume(20)
p accumulator.resume(5)

# alive? state tracking
f = Fiber.new { 1 + 1 }
p f.alive?
f.resume
p f.alive?
