path = "/tmp/stoned_file_gets_limit.txt"
File.write(path, "alpha\nbeta\n\ngamma")

f = File.open(path, "r")
puts f.gets(0).inspect
f.rewind
puts f.gets(2).inspect
f.rewind
puts f.gets("\n", 2).inspect
f.rewind
puts f.gets(nil, 2).inspect
f.rewind
puts f.gets("", 7).inspect
f.rewind
puts f.gets(-1).inspect

begin
  f.gets(true)
rescue => e
  puts e.class
  puts e.message
end

begin
  f.gets("\n", true)
rescue => e
  puts e.class
  puts e.message
end

f.close
File.delete(path)
