path = "/tmp/stoned_io_eof_predicate.txt"
File.write(path, "abc")

f = File.open(path, "r")
io = IO.new(f.fileno, "r")

puts io.eof?
puts io.read(3).inspect
puts io.eof?
io.rewind
puts io.eof?

begin
  io.eof?(1)
rescue => e
  puts e.class
  puts e.message
end

io.close

begin
  io.eof?
rescue => e
  puts e.class
  puts e.message
end

f.close
File.delete(path)
