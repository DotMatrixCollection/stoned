path = "/tmp/stoned_io_seek_type_errors.txt"
File.write(path, "abc")
f = File.open(path, "r")

begin
  f.seek("x")
rescue => e
  puts e.class
  puts e.message
end

begin
  f.seek(nil)
rescue => e
  puts e.class
  puts e.message
end

begin
  f.seek(true)
rescue => e
  puts e.class
  puts e.message
end

begin
  f.seek(0, [])
rescue => e
  puts e.class
  puts e.message
end

f.close
File.delete(path)
