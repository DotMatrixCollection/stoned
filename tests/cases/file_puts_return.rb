path = "/tmp/stoned_file_puts_return.txt"

f = File.open(path, "w")
puts f.puts == nil
puts f.puts("ab", [3, 4]) == nil
puts f.write("cd")
f.close

puts File.read(path).inspect
File.delete(path)
