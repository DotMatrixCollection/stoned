io_w = IO.new(1, "w")
begin
  io_w.read
rescue => e
  puts e.class
  puts e.message
end

begin
  puts io_w.gets.inspect
rescue => e
  puts e.class
  puts e.message
end

io_r = IO.new(0, "r")
begin
  io_r.write("x")
rescue => e
  puts e.class
  puts e.message
end
