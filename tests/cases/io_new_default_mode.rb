path = "/tmp/stoned_io_new_default_mode.txt"

f = File.open(path, "w")
io = IO.new(f.fileno)
puts io.write("xy")
io.close
f.close
puts File.read(path).inspect

File.write(path, "abc")
f = File.open(path, "r")
io = IO.new(f.fileno, nil)
puts io.read.inspect
io.close
f.close

File.delete(path)
