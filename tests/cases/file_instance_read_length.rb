path = "/tmp/stoned_file_instance_read_length.txt"
File.write(path, "ab")
f = File.open(path, "r")

puts f.read(0).inspect
puts f.read(1).inspect
puts f.read(1).inspect
puts f.read(1).inspect
puts f.read.inspect
puts f.read(0).inspect

f.close
File.delete(path)
