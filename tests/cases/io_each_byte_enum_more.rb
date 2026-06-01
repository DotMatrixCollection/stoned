path = "/tmp/stoned_io_each_byte.txt"
File.write(path, "ab")
f = File.open(path, "r")
p f.each_byte.to_a
f.close
File.delete(path)
