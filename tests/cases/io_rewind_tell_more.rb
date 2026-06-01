path = "/tmp/stoned_io_rewind.txt"
File.write(path, "abcdef")
f = File.open(path, "r")
p f.read(2)
p f.tell
f.rewind
p f.read(3)
f.close
File.delete(path)
