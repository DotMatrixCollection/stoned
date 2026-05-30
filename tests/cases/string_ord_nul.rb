puts "\x00".ord
puts "\x00A".ord

begin
  puts "".ord
rescue ArgumentError => e
  puts e.class
  puts e.message
end
