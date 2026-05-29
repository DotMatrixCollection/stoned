puts defined?(RangeError)
puts defined?(FloatDomainError)
puts FloatDomainError.superclass == RangeError

puts 0.1.to_r.inspect
puts (1.0 / 3.0).to_r.inspect
puts (-0.125).to_r.inspect

puts 0.1.rationalize.inspect
puts (1.0 / 3.0).rationalize.inspect
puts 0.3.rationalize(0.05).inspect
puts (-0.3).rationalize(0.05).inspect
puts 0.3.rationalize(0).inspect

begin
  Float::INFINITY.to_r
rescue => e
  puts e.class
  puts e.message
end

begin
  Float::NAN.rationalize
rescue => e
  puts e.class
  puts e.message
end

begin
  0.3.rationalize(Object.new)
rescue => e
  puts e.class
  puts e.message
end
