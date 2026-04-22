begin
  raise ArgumentError, "bad"
rescue TypeError
  puts "wrong"
rescue ArgumentError
  puts "right"
end
