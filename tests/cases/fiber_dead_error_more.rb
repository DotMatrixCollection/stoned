f = Fiber.new { :done }
p f.resume
begin
  f.resume
rescue FiberError => e
  puts e.class
end
