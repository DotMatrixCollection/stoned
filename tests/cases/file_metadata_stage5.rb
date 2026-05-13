base = "/tmp/stoned_file_metadata_stage5"
file = File.join(base, "note.txt")

begin
  Dir.mkdir(base)
rescue Errno::EEXIST
end

File.write(file, "hello")

puts File.directory?(base)
puts File.directory?(file)
puts File.file?(file)
puts File.file?(base)
puts File.readable?(file)
puts File.writable?(file)
puts File.executable?(file)

t1 = File.mtime(file)
t2 = File.mtime(file)

puts t1.class == Time
puts t1 == t2
puts t1 != t2
puts(t1 <=> t2)
puts t1.to_i.class == Integer
