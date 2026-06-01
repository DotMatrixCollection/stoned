require "tempfile"

t = Tempfile.new("closebang")
path = t.path
p File.exist?(path)
t.close!
p t.closed?
p File.exist?(path)
