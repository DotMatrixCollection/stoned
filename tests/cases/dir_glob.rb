files = Dir.glob("src/*.c")
puts files.length > 10
puts files.all? { |f| f.end_with?(".c") }
puts files.any? { |f| f.include?("eval") }

empty = Dir.glob("src/*.nonexistent")
puts empty.length

puts Dir.exist?("src")
puts Dir.exist?("no_such_directory_xyz")
puts Dir.home == ENV["HOME"]

children = Dir.children("src")
puts children.length > 10
puts children.include?("eval.c")
puts children.none? { |e| e == "." || e == ".." }

entries = Dir.entries("src")
puts entries.include?(".")
puts entries.include?("..")
