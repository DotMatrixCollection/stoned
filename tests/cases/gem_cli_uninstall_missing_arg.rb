$stdout.sync = true

require "rbconfig"

def capture(command)
  output = `#{command} 2>&1`.chomp
  [output, $?.exitstatus]
end

gem_exe = File.expand_path("exe/gem", Dir.pwd)
out, status = capture("#{RbConfig.ruby.inspect} #{gem_exe.inspect} uninstall")
puts out
puts status
