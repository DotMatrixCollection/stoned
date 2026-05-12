path = "/tmp/stoned_binary_rw_test.bin"

f = File.open(path, "wb")
puts f.mode
f.write("hello")
f.close

f = File.open(path, "rb")
puts f.mode
puts f.read
f.close

f = File.open(path, "ab")
puts f.mode
f.write(" world")
f.close

f = File.open(path, "rb")
puts f.read
f.close

begin
  f = File.open(path, "rb")
  f.write("x")
rescue IOError => e
  puts e.message
  f.close
end

begin
  f = File.open(path, "wb")
  f.read
rescue IOError => e
  puts e.message
  f.close
end

File.delete(path)
