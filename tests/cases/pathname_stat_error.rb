require "pathname"

path = "/tmp/stoned_pathname_error_missing"
begin
  Pathname.new(path).stat
rescue Errno::ENOENT => e
  puts e.class
end
