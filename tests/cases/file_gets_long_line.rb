path = "/tmp/stoned_file_gets_long_line.txt"

f = File.open(path, "w")
i = 0
while i < 5000
  f.write("a")
  i = i + 1
end
f.write("\nrest\n")
f.close

f = File.open(path, "r")
line = f.gets
puts line.length
puts line[-1]
puts f.gets.inspect
puts f.gets.inspect
f.close

File.delete(path)
