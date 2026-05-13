path = "/tmp/stoned_file_plus_modes.txt"
File.write(path, "abc")

f = File.open(path, "r+")
puts f.read.inspect
f.rewind
puts f.write("XYZ")
f.rewind
puts f.read.inspect
f.close

File.delete(path)
