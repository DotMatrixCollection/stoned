path = "/tmp/example/archive.tar.gz"

p File.basename(path)
p File.basename(path, ".gz")
p File.basename(path, ".*")
p File.dirname(path)
p File.extname(path)
p File.join("/tmp", "example", "archive.tar.gz")
