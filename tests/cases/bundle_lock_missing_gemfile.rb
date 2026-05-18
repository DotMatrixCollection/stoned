$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_lock_missing_gemfile_#{$$}"
app = File.join(root, "app")
[root, app].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }

def capture(command)
  output = `#{command} 2>&1`.chomp
  [output, $?.exitstatus]
end

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

Dir.chdir(app) do
  out, status = capture("#{RbConfig.ruby.inspect} #{bundle_exe.inspect} lock")
  puts out
  puts status
end

system("rm", "-rf", root)
