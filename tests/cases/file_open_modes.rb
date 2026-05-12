path = "/tmp/stoned_file_modes.txt"

File.open(path, "w") { |f|
  puts f.mode
  puts f.write("a")
  puts f.print("b", "c")
  puts f.puts("d", ["e", "f"])
}

File.open(path, "a") { |f|
  puts f.mode
  puts f.tell
  puts f.write("g")
  puts f.tell
}

f = File.open(path, "r")
puts f.mode
puts f.read
f.close

begin
  f.read
rescue IOError => e
  puts e.message
end
