p Time.now.to_i > 0
t = File.mtime("README.md")
p t.class
p t == t
