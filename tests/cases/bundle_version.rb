$stdout.sync = true

require "rbconfig"

def capture(command)
  output = `#{command} 2>&1`.chomp
  [output, $?.exitstatus]
end

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
ruby_cmd = RbConfig.ruby.inspect

out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} version")
puts out
puts status

out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} --version")
puts out
puts status

out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} -v")
puts out
puts status
