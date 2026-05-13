path = "/tmp/stoned_file_getc.txt"
File.write(path, "éx")

f = File.open(path, "r")
puts f.getc.inspect
puts f.pos
puts f.getc.inspect
puts f.getc.inspect
f.rewind
puts f.getc.inspect

begin
  f.getc(1)
rescue => e
  puts e.class
  puts e.message
end

f.close

begin
  f.getc
rescue => e
  puts e.class
  puts e.message
end

File.delete(path)
