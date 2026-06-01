path = "/tmp/stoned_file_open_delete.txt"

File.open(path, "w") { |f| f.write("gamma") }

result = File.open(path) { |f|
  puts f.path
  puts f.closed?
  puts f.read
  puts f.closed?
  "block-result"
}

puts result

f = File.open(path)
puts f.closed?
puts f.read
puts f.close
puts f.closed?
puts File.respond_to?(:delete)
puts File.respond_to?(:unlink)
puts File.methods.include?(:delete)
puts File.delete(path)
puts File.exist?(path)
