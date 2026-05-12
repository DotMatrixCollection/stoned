path = "/tmp/stoned_file_print_return.txt"

f = File.open(path, "w")
puts f.print == nil
puts f.print("ab", 3) == nil
puts f.write("cd")
f.close

puts File.read(path).inspect
File.delete(path)
