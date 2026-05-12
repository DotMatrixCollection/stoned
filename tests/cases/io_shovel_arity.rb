path = "/tmp/stoned_io_shovel_arity.txt"

f = File.open(path, "w")
begin
  f.public_send("<<")
rescue => e
  puts e.class
  puts e.message
end
begin
  f.public_send("<<", "a", "b")
rescue => e
  puts e.class
  puts e.message
end
f.close
File.delete(path)

io = IO.new(1, "w")
begin
  io.public_send("<<")
rescue => e
  puts e.class
  puts e.message
end
begin
  io.public_send("<<", "a", "b")
rescue => e
  puts e.class
  puts e.message
end
io.close
