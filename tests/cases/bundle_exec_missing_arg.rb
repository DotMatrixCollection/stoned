$stdout.sync = true

require "rbconfig"

def capture(command)
  output = `#{command} 2>&1`.chomp
  [output, $?.exitstatus]
end

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
out, status = capture("#{RbConfig.ruby.inspect} #{bundle_exe.inspect} exec")
puts out
puts status
