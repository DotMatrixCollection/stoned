path = "/tmp/stoned_file_open_perm_arg.txt"

File.open(path, "w", 0644) { |f| puts f.write("x") }
puts File.read(path).inspect
File.delete(path)

File.open(path, "w", nil) { |f| puts f.write("y") }
puts File.read(path).inspect
File.delete(path)

begin
  File.open(path, "w", true)
rescue => e
  puts e.class
  puts e.message
end
