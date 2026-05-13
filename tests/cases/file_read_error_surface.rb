path = "/tmp/stoned_file_read_error_surface.txt"
File.write(path, "abc")

begin
  File.read(path, 1, 2, 3)
rescue => e
  puts e.class
  puts e.message
end

begin
  File.read(path, 2, -1)
rescue => e
  puts e.class
  puts e.message
end

File.delete(path)
