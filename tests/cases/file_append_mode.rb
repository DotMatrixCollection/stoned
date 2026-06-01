path = "/tmp/stoned_file_write_append.txt"
File.write(path, "one")
File.open(path, "a") { |f| f.write("two") }
p File.read(path)
File.delete(path)
