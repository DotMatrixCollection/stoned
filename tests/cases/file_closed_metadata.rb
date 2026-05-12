path = "/tmp/stoned_file_closed_metadata.txt"

f = File.open(path, "w")
f.close

puts f.path
puts f.mode

begin
  f.tell
rescue => e
  puts e.class
  puts e.message
end

File.delete(path)
