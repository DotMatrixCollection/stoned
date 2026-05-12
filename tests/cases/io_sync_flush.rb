path = "/tmp/stoned_io_sync_flush.txt"

f = File.open(path, "w")
f.write("a")
puts File.read(path).inspect
f.sync = true
f.write("b")
puts File.read(path).inspect
f.print("c")
puts File.read(path).inspect
f.puts("d")
puts File.read(path).inspect
f.close

File.delete(path)
