$stdout.sync = true

require "rbconfig"

def capture(command)
  output = `#{command} 2>&1`.chomp
  [output, $?.exitstatus]
end

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
ruby_cmd = RbConfig.ruby.inspect

# config get with no key
out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} config get")
puts out
puts status

# config set with missing value
puts "--SET-NO-VALUE--"
out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} config set WITHOUT")
puts out
puts status

# config unset with no key
puts "--UNSET-NO-KEY--"
out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} config unset")
puts out
puts status
