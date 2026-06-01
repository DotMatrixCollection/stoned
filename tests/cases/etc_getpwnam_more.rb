require "etc"

pw = Etc.getpwnam("nobody")
p pw.name.length > 0
p pw.dir.start_with?("/")
