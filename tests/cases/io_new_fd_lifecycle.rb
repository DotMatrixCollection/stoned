io = IO.new(1, "w")
puts io.close
puts io.closed?
puts io.close

begin
  puts io.fileno
rescue => e
  puts e.class
  puts e.message
end

begin
  puts io.tell
rescue => e
  puts e.class
  puts e.message
end
