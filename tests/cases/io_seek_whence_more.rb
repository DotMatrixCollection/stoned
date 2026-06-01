path = "/tmp/stoned_io_seek.txt"
File.write(path, "abcdef")
f = File.open(path, "r")
f.seek(2)
p f.pos
p f.read(2)
f.seek(-2, IO::SEEK_END)
p f.read
f.close
File.delete(path)
