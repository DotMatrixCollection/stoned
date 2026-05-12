fixture = "tests/fixtures/binary_sample.bin"

begin
  f = File.open(fixture, "r")
  f.read
rescue EncodingError => e
  puts e.class
end

f = File.open(fixture, "rb")
puts f.mode
begin
  f.read
  puts "no error"
rescue EncodingError => e
  puts "unexpected: #{e.class}"
ensure
  f.close
end
