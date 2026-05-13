path = "/tmp/stoned_file_seek_errno.txt"
File.write(path, "abc")
f = File.open(path, "r")

begin
  f.seek(0, 9)
rescue => e
  puts e.class
  puts e.message
end

f.close
File.delete(path)
