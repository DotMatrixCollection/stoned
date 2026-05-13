File.write("/tmp/stoned_file_delete_1.txt", "")
File.write("/tmp/stoned_file_delete_2.txt", "")
puts File.delete("/tmp/stoned_file_delete_1.txt", "/tmp/stoned_file_delete_2.txt")

File.write("/tmp/stoned_file_delete_3.txt", "")
begin
  File.delete("/tmp/stoned_file_delete_3.txt", "/tmp/stoned_missing_delete_3.txt")
rescue => e
  puts e.class
  puts e.message
end
puts File.exist?("/tmp/stoned_file_delete_3.txt")

begin
  File.exist?
rescue => e
  puts e.class
  puts e.message
end

begin
  File.exist?("a", "b")
rescue => e
  puts e.class
  puts e.message
end
