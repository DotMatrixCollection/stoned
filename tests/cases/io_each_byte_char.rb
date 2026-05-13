path = "/tmp/stoned_io_each_byte_char.txt"
File.write(path, "éx")

f = File.open(path, "r")
io = IO.new(f.fileno, "r")
io.each_byte { |b| puts b }
io.rewind
io.each_char { |ch| puts ch.inspect }
puts io.each_byte { }.class

begin
  io.each_byte(1) { }
rescue => e
  puts e.class
  puts e.message
end

begin
  io.each_char
rescue => e
  puts e.class
  puts e.message
end

io.close
f.close
File.delete(path)
