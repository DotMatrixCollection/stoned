path = "/tmp/stoned_io_gets_limit.txt"
File.write(path, "alpha\nbeta\n\ngamma")
f = File.open(path, "r")
io = IO.new(f.fileno, "r")

puts io.gets(0).inspect
io.rewind
puts io.gets(2).inspect
io.rewind
puts io.gets("\n", 2).inspect
io.rewind
puts io.gets(nil, 2).inspect
io.rewind
puts io.gets("", 7).inspect
io.rewind
puts io.gets(-1).inspect

begin
  io.gets(true)
rescue => e
  puts e.class
  puts e.message
end

begin
  io.gets("\n", true)
rescue => e
  puts e.class
  puts e.message
end

io.close
f.close
File.delete(path)
