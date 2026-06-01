path = "/tmp/stoned_file_each_line.txt"
File.write(path, "alpha\nbeta\ngamma\n")

f = File.open(path, "r")
f.each_line { |l| print l }
f.rewind
f.each_line("\n") { |l| print l.chomp + "!" }
puts
f.rewind
puts f.each_line { }.class
f.rewind
line_enum = f.each_line
puts line_enum.class
p line_enum.to_a
f.rewind
line_enum = f.each_line("\n")
puts line_enum.class
p line_enum.to_a

f.close
File.delete(path)
