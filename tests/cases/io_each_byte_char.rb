path = "/tmp/stoned_io_each_byte_char.txt"
File.write(path, "éx")

f = File.open(path, "r")
io = IO.new(f.fileno, "r")
io.each_byte { |b| puts b }
io.rewind
io.each_char { |ch| puts ch.inspect }
puts io.each_byte { }.class
io.rewind
byte_enum = io.each_byte
puts byte_enum.class
p byte_enum.to_a
io.rewind
char_enum = io.each_char
puts char_enum.class
p char_enum.to_a

begin
  io.each_byte(1) { }
rescue => e
  puts e.class
  puts e.message
end

io.close
f.close
File.delete(path)
