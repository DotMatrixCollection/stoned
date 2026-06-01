path = "/tmp/stoned_file_delete_many_a.txt"
path2 = "/tmp/stoned_file_delete_many_b.txt"
File.write(path, "a")
File.write(path2, "b")
p File.delete(path, path2)
p File.exist?(path)
