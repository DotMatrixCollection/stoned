io = IO.new(1, "w")

begin
  io.seek(0, 9)
rescue => e
  puts e.class
  puts e.message
end

puts io.tell

begin
  io.seek
rescue => e
  puts e.class
  puts e.message
end

io.close
