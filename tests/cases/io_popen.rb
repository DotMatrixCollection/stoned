out = IO.popen("echo hello") { |f| f.read.chomp }
p out

lines = IO.popen("printf 'x\\ny\\nz'") { |f| f.read.split("\n") }
p lines
