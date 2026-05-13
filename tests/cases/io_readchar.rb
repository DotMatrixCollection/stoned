path = "/tmp/stoned_io_readchar.txt"
File.write(path, "éx")

f = File.open(path, "r")
io = IO.new(f.fileno, "r")

puts io.readchar.inspect
puts io.readchar.inspect

begin
  puts io.readchar.inspect
rescue => e
  puts e.class
  puts e.message
end

begin
  io.readchar(1)
rescue => e
  puts e.class
  puts e.message
end

io.close

begin
  io.readchar
rescue => e
  puts e.class
  puts e.message
end

f.close
File.delete(path)
