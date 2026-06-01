require "tempfile"

t = Tempfile.new("pref", suffix: ".txt")
puts t.path.end_with?(".txt")
t.close!
