path = "/tmp/stoned_file_each_byte_char.txt"
File.write(path, "éx")

f = File.open(path, "r")
f.each_byte { |b| puts b }
f.rewind
f.each_char { |ch| puts ch.inspect }
puts f.each_byte { }.class
f.rewind
byte_enum = f.each_byte
puts byte_enum.class
p byte_enum.to_a
f.rewind
char_enum = f.each_char
puts char_enum.class
p char_enum.to_a

begin
  f.each_byte(1) { }
rescue => e
  puts e.class
  puts e.message
end

f.close
File.delete(path)
