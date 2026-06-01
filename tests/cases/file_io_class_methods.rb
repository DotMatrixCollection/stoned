$stdout.sync = true

path = "/tmp/stoned_file_io_class_methods.txt"
File.write(path, "hello world\n")

puts File.size(path)     # 12
puts File.zero?(path)    # false

empty = "/tmp/stoned_file_io_class_empty.txt"
File.write(empty, "")
puts File.size(empty)    # 0
puts File.zero?(empty)   # true

puts File.respond_to?(:size)
puts File.respond_to?(:zero?)
puts File.methods.include?(:size)

puts IO.read(path)       # hello world\n (already has newline)

lines = []
IO.foreach(path) { |l| lines << l.chomp }
puts lines.inspect       # ["hello world"]

File.delete(path)
File.delete(empty)
