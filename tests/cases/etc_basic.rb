require "etc"

puts Etc.sysconfdir
p Etc.nprocessors >= 1
p Etc.getlogin.length > 0
pw = Etc.getpwuid
p pw.name.length > 0
p pw.dir.start_with?("/")
