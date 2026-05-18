$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_show_missing_lockfile_#{$$}"
app = File.join(root, "app")
[root, app].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "demo"
GEMFILE

def capture(command)
  output = `#{command} 2>&1`.chomp
  [output, $?.exitstatus]
end

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

Dir.chdir(app) do
  out, status = capture("#{RbConfig.ruby.inspect} #{bundle_exe.inspect} show")
  puts out
  puts status
end

system("rm", "-rf", root)
