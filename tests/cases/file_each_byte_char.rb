path = "/tmp/stoned_file_each_byte_char.txt"
File.write(path, "éx")

f = File.open(path, "r")
f.each_byte { |b| puts b }
f.rewind
f.each_char { |ch| puts ch.inspect }
puts f.each_byte { }.class

begin
  f.each_byte(1) { }
rescue => e
  puts e.class
  puts e.message
end

begin
  f.each_char
rescue => e
  puts e.class
  puts e.message
end

f.close
File.delete(path)
