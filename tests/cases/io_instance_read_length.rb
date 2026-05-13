path = "/tmp/stoned_io_instance_read_length.txt"
File.write(path, "ab")
f = File.open(path, "r")
io = IO.new(f.fileno, "r")

puts io.read(0).inspect
puts io.read(1).inspect
puts io.read(1).inspect
puts io.read(1).inspect
puts io.read.inspect
puts io.read(0).inspect

io.close
f.close
File.delete(path)
