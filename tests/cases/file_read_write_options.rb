path = "/tmp/stoned_file_read_write_options.txt"
File.write(path, "abcdef")

puts File.read(path).inspect
puts File.read(path, 2).inspect
puts File.read(path, 2, 1).inspect
puts File.read(path, nil, 1).inspect

puts File.write(path, "Z", 1)
puts File.read(path).inspect
puts File.write(path, "Q", nil)
puts File.read(path).inspect

File.delete(path)
