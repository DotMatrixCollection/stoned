begin
  raise ArgumentError, "bad"
rescue ArgumentError
  puts "rescued"
end

puts "after"
