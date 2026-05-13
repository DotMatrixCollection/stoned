path = "/tmp/stoned_io_bytes.txt"
File.write(path, "é")

f = File.open(path, "rb")
io = IO.new(f.fileno, "rb")

puts io.getbyte
puts io.getbyte
puts io.getbyte.inspect
io.rewind
puts io.readbyte
puts io.readbyte

begin
  puts io.readbyte.inspect
rescue => e
  puts e.class
  puts e.message
end

begin
  io.getbyte(1)
rescue => e
  puts e.class
  puts e.message
end

begin
  io.readbyte(1)
rescue => e
  puts e.class
  puts e.message
end

io.close

begin
  io.getbyte
rescue => e
  puts e.class
  puts e.message
end

begin
  io.readbyte
rescue => e
  puts e.class
  puts e.message
end

f.close
File.delete(path)
