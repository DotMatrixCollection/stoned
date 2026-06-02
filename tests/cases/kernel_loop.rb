# Kernel#loop runs forever until break or StopIteration
n = 0
result = loop do
  n += 1
  break n * 10 if n == 5
end
p result
p n

# loop rescues StopIteration from enumerator
enum = [1, 2, 3].each
collected = []
loop do
  collected << enum.next
end
p collected

# loop with next to skip
evens = []
i = 0
loop do
  i += 1
  break if i > 10
  next if i.odd?
  evens << i
end
p evens

# loop returns nil when no break value
counter = 0
r = loop do
  counter += 1
  break if counter >= 3
end
p r

# Infinite generator using loop + Fiber
gen = Fiber.new do
  n = 0
  loop { Fiber.yield(n += 1) }
end
p 5.times.map { gen.resume }
