path = "/tmp/stoned_file_eof_predicate.txt"
File.write(path, "abc")

f = File.open(path, "r")
puts f.eof?
puts f.read(3).inspect
puts f.eof?
f.rewind
puts f.eof?

begin
  f.eof?(1)
rescue => e
  puts e.class
  puts e.message
end

f.close

begin
  f.eof?
rescue => e
  puts e.class
  puts e.message
end

File.delete(path)
