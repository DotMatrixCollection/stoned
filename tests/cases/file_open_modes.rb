path = "/tmp/stoned_file_modes.txt"

File.open(path, "w") { |f|
  puts f.mode
  puts f.write("a")
  puts f.print("b", "c")
  puts f.puts("d", ["e", "f"])
}

File.open(path, "a") { |f|
  puts f.mode
  puts f.write("g")
}

f = File.open(path, "r")
puts f.mode
puts f.read
f.close

begin
  f.read
rescue LoadError => e
  puts e.message
end
