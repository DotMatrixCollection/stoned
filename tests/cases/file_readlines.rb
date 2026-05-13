path = "/tmp/stoned_file_readlines.txt"
File.write(path, "a\nb\n\n")

f = File.open(path, "r")
f.readlines.each { |line| puts line.inspect }
puts "--"
f.rewind
f.readlines(2).each { |line| puts line.inspect }
puts "--"
f.rewind
f.readlines(nil, 2).each { |line| puts line.inspect }
puts "--"
f.rewind
f.readlines("", 3).each { |line| puts line.inspect }
puts "--"

begin
  f.readlines(true)
rescue => e
  puts e.class
  puts e.message
end

begin
  f.readlines("\n", true)
rescue => e
  puts e.class
  puts e.message
end

begin
  f.readlines(1, 2, 3)
rescue => e
  puts e.class
  puts e.message
end

f.close
File.delete(path)
