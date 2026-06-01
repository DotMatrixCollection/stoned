f = Fiber.new do
  Fiber.yield 1
  Fiber.yield 2
  3
end

p f.resume
p f.resume
p f.resume
p f.alive?

begin
  f.resume
rescue FiberError => e
  p e.message
end
