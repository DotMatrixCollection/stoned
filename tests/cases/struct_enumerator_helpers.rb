Point = Struct.new(:x, :y)
p = Point.new(3, 4)

puts p.size
puts p.length
p p.values

puts p.each.class
p p.each.to_a

puts p.each_pair.class
p p.each_pair.to_a

p p.values_at(1, :x, -1, 5)

begin
  p[:z]
rescue => e
  puts e.class
  puts e.message
end

begin
  p[:z] = 9
rescue => e
  puts e.class
  puts e.message
end

begin
  p.values_at(:z)
rescue => e
  puts e.class
  puts e.message
end
