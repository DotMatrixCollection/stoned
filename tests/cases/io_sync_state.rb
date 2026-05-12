path = "/tmp/stoned_io_sync_state.txt"

f = File.open(path, "w")
puts f.sync.inspect
puts (f.sync = true).inspect
puts f.sync.inspect
f.close
begin
  puts f.sync.inspect
rescue => e
  puts e.class
  puts e.message
end
File.delete(path)

io = IO.new(1, "w")
puts io.sync.inspect
puts (io.sync = true).inspect
puts io.sync.inspect
io.close
begin
  puts io.sync.inspect
rescue => e
  puts e.class
  puts e.message
end
