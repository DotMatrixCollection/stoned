path = "/tmp/stoned_file_each_line.txt"
File.write(path, "alpha\nbeta\ngamma\n")

f = File.open(path, "r")
f.each_line { |l| print l }
f.rewind
f.each_line("\n") { |l| print l.chomp + "!" }
puts
f.rewind
puts f.each_line { }.class

begin
  f.each_line
rescue => e
  puts e.class
  puts e.message
end

f.close
File.delete(path)
