begin
  File.read(nil)
rescue => e
  puts e.class
  puts e.message
end

begin
  File.delete(true)
rescue => e
  puts e.class
  puts e.message
end

begin
  File.exist?([])
rescue => e
  puts e.class
  puts e.message
end

begin
  File.open(:x)
rescue => e
  puts e.class
  puts e.message
end

puts File.write("/tmp/stoned_file_api_coercion.txt", nil)
puts File.read("/tmp/stoned_file_api_coercion.txt").inspect
puts File.write("/tmp/stoned_file_api_coercion.txt", true)
puts File.read("/tmp/stoned_file_api_coercion.txt").inspect
File.delete("/tmp/stoned_file_api_coercion.txt")
