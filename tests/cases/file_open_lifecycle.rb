path = "/tmp/stoned_file_open_lifecycle.txt"
File.write(path, "abc")

f = nil
result = File.open(path) { |io| f = io; io.read }
puts result
puts f.closed?

f = nil
result = File.open(path) { |io| f = io; next :nexted }
puts result.inspect
puts f.closed?

f = nil
result = File.open(path) { |io| f = io; break :broke }
puts result.inspect
puts f.closed?

f = nil
begin
  File.open(path) { |io| f = io; raise "boom" }
rescue => e
  puts e.class
  puts e.message
end
puts f.closed?

def file_open_return(path, holder)
  File.open(path) { |io| holder << io; return :ret }
end

holder = []
puts file_open_return(path, holder)
puts holder[0].closed?

f = nil
result = File.open(path) do |io|
  f = io
  puts io.close.inspect
  :manual_close
end
puts result.inspect
puts f.closed?

f = File.open(path)
puts f.close.inspect
puts f.closed?
puts f.close.inspect

File.delete(path)
