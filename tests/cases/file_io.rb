path = "/tmp/stoned_io_test.txt"

puts File.write(path, "alpha\nbeta")
puts File.exist?(path)
puts File.read(path)
