path = "/tmp/stoned_file_readline.txt"
File.write(path, "abc\n\ndef")

f = File.open(path, "r")
puts f.readline.inspect
puts f.readline(2).inspect
f.rewind
puts f.readline(nil, 2).inspect
f.rewind
puts f.readline("", 5).inspect

begin
  f.readline(true)
rescue => e
  puts e.class
  puts e.message
end

begin
  f.readline("\n", true)
rescue => e
  puts e.class
  puts e.message
end

f.read

begin
  f.readline
rescue => e
  puts e.class
  puts e.message
end

f.close
File.delete(path)
