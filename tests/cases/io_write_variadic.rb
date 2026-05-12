path = "/tmp/stoned_io_write_variadic.txt"

f = File.open(path, "w")
puts f.write
puts f.write(nil)
puts f.write("a", "b", 3)
f.close
puts File.read(path).inspect
File.delete(path)

io = IO.new(1, "w")
io_path = "/tmp/stoned_io_write_variadic_io.txt"
backing = File.open(io_path, "w")
io = IO.new(backing.fileno, "w")
puts io.write
puts io.write(nil)
puts io.write("x", "y", 4)
io.close
backing.close
puts File.read(io_path).inspect
File.delete(io_path)
