puts Array.new.inspect
puts Array.new(3).inspect
puts Array.new(3, "x").inspect
puts Array.new(4) { |i| i * 2 }.inspect

shared = []
arr = Array.new(3, shared)
arr[0] << 1
puts arr.inspect

begin
  Array.new(-1)
rescue ArgumentError => e
  puts e.message
end
