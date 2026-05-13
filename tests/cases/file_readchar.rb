path = "/tmp/stoned_file_readchar.txt"
File.write(path, "éx")

f = File.open(path, "r")
puts f.readchar.inspect
puts f.readchar.inspect

begin
  puts f.readchar.inspect
rescue => e
  puts e.class
  puts e.message
end

begin
  f.readchar(1)
rescue => e
  puts e.class
  puts e.message
end

f.close

begin
  f.readchar
rescue => e
  puts e.class
  puts e.message
end

File.delete(path)
