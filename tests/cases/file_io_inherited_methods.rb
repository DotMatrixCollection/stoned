path = "/tmp/stoned_file_io_inherited_methods.txt"

f = File.open(path, "w")
puts f.sync.inspect
puts (f.sync = true).inspect
puts f.sync.inspect
puts f.flush == f
puts f.fileno.class
puts f.tty?
f.close

begin
  f.flush
rescue => e
  puts e.class
  puts e.message
end

File.delete(path)
