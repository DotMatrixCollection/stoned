path = "/tmp/stoned_file_metadata_more.txt"
File.write(path, "abc")
p File.size(path)
p File.zero?(path)
p File.file?(path)
p File.directory?("/tmp")
File.delete(path)
