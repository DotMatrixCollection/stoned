require "pathname"

path = "/tmp/stoned_pathname_stat_chmod.txt"
File.write(path, "x")
p = Pathname.new(path)
p p.exist?
p p.writable?
p p.stat.class.to_s
p p.chmod(0600)
File.delete(path)
