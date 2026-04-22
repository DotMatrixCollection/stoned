begin
  attr_reader :name
rescue TypeError => e
  puts e.class
  puts e.message
end
