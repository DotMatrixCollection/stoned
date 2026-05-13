path = "/tmp/stoned_io_new_plus_mode.txt"
File.write(path, "abc")

f = File.open(path, "r+")
io = IO.new(f.fileno, "r+")
puts io.read.inspect
io.rewind
puts io.write("XYZ")
io.rewind
puts io.read.inspect

File.delete(path)
