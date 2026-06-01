path = "/tmp/stoned_file_open_block.txt"
result = File.open(path, "w") { |f| f.write("abc") }
p result
p File.read(path)
File.delete(path)
