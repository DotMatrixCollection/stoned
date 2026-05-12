path = "/tmp/stoned_file_seek_tell.txt"

File.open(path, "w") do |f|
  puts f.tell
  puts f.write("abcdef")
  puts f.tell
  puts f.rewind
  puts f.tell
  puts f.write("XYZ")
  puts f.tell
  puts f.seek(-1, SEEK_CUR)
  puts f.tell
end

File.open(path, "r") do |f|
  puts f.tell
  puts f.read
  puts f.tell
  puts f.rewind
  puts f.tell
  puts f.seek(2, SEEK_SET)
  puts f.tell
  puts f.read
  puts f.tell
  puts f.seek(-1, SEEK_END)
  puts f.tell
  puts f.read
end

puts File.delete(path)
