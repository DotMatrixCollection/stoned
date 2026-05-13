path = "/tmp/stoned_file_bytes.txt"
File.write(path, "é")

f = File.open(path, "rb")
puts f.getbyte
puts f.getbyte
puts f.getbyte.inspect
f.rewind
puts f.readbyte
puts f.readbyte

begin
  puts f.readbyte.inspect
rescue => e
  puts e.class
  puts e.message
end

begin
  f.getbyte(1)
rescue => e
  puts e.class
  puts e.message
end

begin
  f.readbyte(1)
rescue => e
  puts e.class
  puts e.message
end

f.close

begin
  f.getbyte
rescue => e
  puts e.class
  puts e.message
end

begin
  f.readbyte
rescue => e
  puts e.class
  puts e.message
end

File.delete(path)
