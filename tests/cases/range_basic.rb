r = 1..10
puts r.begin
puts r.end
puts r.exclude_end?
puts r.to_s

r2 = 1...10
puts r2.begin
puts r2.end
puts r2.exclude_end?
puts r2.to_s

puts r.is_a?(Range)
puts r.class
