f = Fiber.new do |x|
  y = Fiber.yield(x + 1)
  y + 2
end
p f.alive?
p f.resume(10)
p f.alive?
p f.resume(20)
p f.alive?
