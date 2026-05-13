path = "/tmp/stoned_io_getc.txt"
File.write(path, "éx")

f = File.open(path, "r")
io = IO.new(f.fileno, "r")

puts io.getc.inspect
puts io.pos
puts io.getc.inspect
puts io.getc.inspect
io.rewind
puts io.getc.inspect

begin
  io.getc(1)
rescue => e
  puts e.class
  puts e.message
end

io.close

begin
  io.getc
rescue => e
  puts e.class
  puts e.message
end

File.delete(path)
