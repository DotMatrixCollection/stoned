path = "/tmp/stoned_io_sync.txt"
f = File.open(path, "w")
p f.sync
f.sync = true
p f.sync
f.write("x")
f.close
p File.read(path)
File.delete(path)
