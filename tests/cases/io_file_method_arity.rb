path = "/tmp/stoned_io_file_method_arity.txt"

f = File.open(path, "w")

begin
  f.public_send("close", 1)
rescue => e
  puts e.class
  puts e.message
end
puts f.closed?

begin
  f.public_send("path", 1)
rescue => e
  puts e.class
  puts e.message
end

begin
  f.public_send("sync=")
rescue => e
  puts e.class
  puts e.message
end

io = IO.new(1, "w")

begin
  io.public_send("flush", 1)
rescue => e
  puts e.class
  puts e.message
end

begin
  io.public_send("sync=")
rescue => e
  puts e.class
  puts e.message
end

f.close
File.delete(path)
