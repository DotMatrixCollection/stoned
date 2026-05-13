path = "/tmp/stoned_io_pos_alias.txt"
File.write(path, "abc")

f = File.open(path, "r")
io = IO.new(f.fileno, "r")

puts io.pos
puts (io.pos = 2)
puts io.pos
puts io.read.inspect

begin
  io.pos = true
rescue => e
  puts e.class
  puts e.message
end

begin
  io.pos(-1)
rescue => e
  puts e.class
  puts e.message
end

begin
  io.pos = -1
rescue => e
  puts e.class
  puts e.message
end

io.close

begin
  io.pos
rescue => e
  puts e.class
  puts e.message
end

begin
  io.pos = 0
rescue => e
  puts e.class
  puts e.message
end

f.close
File.delete(path)
