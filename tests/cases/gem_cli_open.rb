$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_gem_cli_open_#{$$}"
project = File.join(root, "demo")
lib_dir = File.join(project, "lib")
gem_home = File.join(root, "gemhome")
[root, project, lib_dir, gem_home].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

File.write(File.join(lib_dir, "demo.rb"), <<~RUBY)
  module Demo
    VALUE = 7
  end
RUBY

File.write(File.join(project, "demo.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "demo"
    s.version = Gem::Version.new("1.2.3")
    s.summary = "demo gem"
    s.files = ["lib/demo.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC

old_gem_home = ENV["GEM_HOME"]
old_home = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"] = root

gem_exe = File.expand_path("exe/gem", Dir.pwd)

def capture(command)
  output = `#{command} 2>&1`.chomp
  [output, $?.exitstatus]
end

Dir.chdir(project) do
  puts `#{RbConfig.ruby} #{gem_exe} build demo.gemspec 2>&1`.chomp
  puts `#{RbConfig.ruby} #{gem_exe} install demo-1.2.3.gem 2>&1`.chomp

  puts "--OPEN--"
  open_out, open_status = capture("#{RbConfig.ruby.inspect} #{gem_exe.inspect} open demo")
  puts open_out
  puts open_status

  puts "--OPEN-MISSING--"
  missing_out, missing_status = capture("#{RbConfig.ruby.inspect} #{gem_exe.inspect} open missing")
  puts missing_out
  puts missing_status
end

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"] = old_home
system("rm", "-rf", root)
