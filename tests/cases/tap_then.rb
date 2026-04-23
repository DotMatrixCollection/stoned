value = "cat".tap { |s| puts s.upcase }
puts value

puts 5.then { |n| n * 3 }
puts 5.yield_self { |n| n + 2 }

begin
  1.tap
rescue LocalJumpError => e
  puts e.class
end

begin
  1.then
rescue LocalJumpError => e
  puts e.class
end
