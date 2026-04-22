begin
  raise ArgumentError, "bad"
rescue ArgumentError => e
  puts e.class
  puts e.message
end
