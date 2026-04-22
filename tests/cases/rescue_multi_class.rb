begin
  raise ArgumentError, "bad"
rescue TypeError, ArgumentError => e
  puts e.class
  puts e.message
end
