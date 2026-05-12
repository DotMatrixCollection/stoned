path = "/tmp/stoned_file_gets_separators.txt"

File.write(path, "alpha\nbeta\n\ngamma")

f = File.open(path, "r")
puts f.gets.inspect
f.rewind
puts f.gets("ta").inspect
f.rewind
puts f.gets("").inspect
f.rewind
puts f.gets(nil).inspect
f.close

File.delete(path)
