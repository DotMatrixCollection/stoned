path = "/tmp/stoned_file_shovel.txt"

f = File.open(path, "w")
puts (f << 123) == f
f.close
puts File.read(path).inspect

begin
  f << "x"
rescue => e
  puts e.class
  puts e.message
end

f = File.open(path, "r")
begin
  f << "x"
rescue => e
  puts e.class
  puts e.message
end
f.close

File.delete(path)
