begin
  raise ArgumentError, "bad"
rescue TypeError
  puts "rescued"
end
