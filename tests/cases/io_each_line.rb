path = "/tmp/stoned_io_each_line.txt"
File.write(path, "alpha\nbeta\ngamma\n")

f = File.open(path, "r")
io = IO.new(f.fileno, "r")
io.each_line { |l| print l }
io.rewind
io.each_line("\n") { |l| print l.chomp + "!" }
puts
io.rewind
puts io.each_line { }.class

begin
  io.each_line
rescue => e
  puts e.class
  puts e.message
end

io.close
f.close
File.delete(path)
