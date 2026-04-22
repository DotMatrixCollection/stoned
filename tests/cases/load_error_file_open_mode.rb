path = "/tmp/stoned_file_open_mode.txt"

begin
  File.open(path, "q")
rescue ArgumentError => e
  puts e.class
  puts e.message
end
