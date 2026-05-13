path = "/tmp/stoned_io_readlines.txt"
File.write(path, "a\nb\n\n")

f = File.open(path, "r")
io = IO.new(f.fileno, "r")

io.readlines.each { |line| puts line.inspect }
puts "--"
io.rewind
io.readlines(2).each { |line| puts line.inspect }
puts "--"
io.rewind
io.readlines(nil, 2).each { |line| puts line.inspect }
puts "--"
io.rewind
io.readlines("", 3).each { |line| puts line.inspect }
puts "--"

begin
  io.readlines(true)
rescue => e
  puts e.class
  puts e.message
end

begin
  io.readlines("\n", true)
rescue => e
  puts e.class
  puts e.message
end

begin
  io.readlines(1, 2, 3)
rescue => e
  puts e.class
  puts e.message
end

io.close
f.close
File.delete(path)
