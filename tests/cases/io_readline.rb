path = "/tmp/stoned_io_readline.txt"
File.write(path, "abc\n\ndef")

f = File.open(path, "r")
io = IO.new(f.fileno, "r")

puts io.readline.inspect
puts io.readline(2).inspect
io.rewind
puts io.readline(nil, 2).inspect
io.rewind
puts io.readline("", 5).inspect

begin
  io.readline(true)
rescue => e
  puts e.class
  puts e.message
end

begin
  io.readline("\n", true)
rescue => e
  puts e.class
  puts e.message
end

io.read

begin
  io.readline
rescue => e
  puts e.class
  puts e.message
end

io.close
f.close
File.delete(path)
