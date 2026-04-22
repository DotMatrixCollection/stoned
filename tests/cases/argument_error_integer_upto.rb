begin
  1.upto do |n|
    puts n
  end
rescue ArgumentError => e
  puts e.class
  puts e.message
end
