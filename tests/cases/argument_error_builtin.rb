begin
  "abc".include?
rescue ArgumentError => e
  puts e.class
  puts e.message
end
