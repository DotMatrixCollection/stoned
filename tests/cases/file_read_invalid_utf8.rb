begin
  File.read("tests/fixtures/encoding/invalid_utf8.txt")
rescue EncodingError => e
  puts e.class
  puts e.message
end
