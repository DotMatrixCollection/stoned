f = Fiber.new { Fiber.yield 1; 2 }
puts f.respond_to?(:resume)
puts f.respond_to?(:alive?)
puts f.methods.include?(:resume)
puts f.methods.include?(:alive?)
puts f.methods.include?(:class)
puts f.methods.include?(:respond_to?)
