begin
  [1, 2, 3].include?
rescue ArgumentError => e
  puts e.class
  puts e.message
end
