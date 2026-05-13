path = "/tmp/stoned_file_pos_alias.txt"
File.write(path, "abc")

f = File.open(path, "r")
puts f.pos
puts (f.pos = 2)
puts f.pos
puts f.read.inspect

begin
  f.pos = true
rescue => e
  puts e.class
  puts e.message
end

begin
  f.pos(-1)
rescue => e
  puts e.class
  puts e.message
end

begin
  f.pos = -1
rescue => e
  puts e.class
  puts e.message
end

f.close

begin
  f.pos
rescue => e
  puts e.class
  puts e.message
end

begin
  f.pos = 0
rescue => e
  puts e.class
  puts e.message
end

File.delete(path)
