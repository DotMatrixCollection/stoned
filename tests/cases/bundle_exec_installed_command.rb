$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_exec_installed_command_#{$$}"
project = File.join(root, "demo")
app = File.join(root, "app")
lib_dir = File.join(project, "lib")
bin_dir = File.join(project, "exe")
gem_home = File.join(root, "gemhome")
[root, project, app, lib_dir, bin_dir, gem_home].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

File.write(File.join(lib_dir, "demo.rb"), "module Demo\n  VALUE = 42\nend\n")
File.write(File.join(bin_dir, "demo-tool"), <<~RUBY)
  require "demo"
  puts Demo::VALUE
  puts ARGV.inspect
RUBY

File.write(File.join(project, "demo.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "demo"
    s.version = Gem::Version.new("1.2.3")
    s.summary = "demo"
    s.files = ["lib/demo.rb", "exe/demo-tool"]
    s.require_paths = ["lib"]
    s.executables = ["demo-tool"]
    s.bindir = "exe"
  end
GEMSPEC

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "demo"
GEMFILE

old_gem_home = ENV["GEM_HOME"]
old_home = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"] = root

gem_exe = File.expand_path("exe/gem", Dir.pwd)
bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

Dir.chdir(project) do
  puts `#{RbConfig.ruby} #{gem_exe} build demo.gemspec 2>&1`.chomp
  puts `#{RbConfig.ruby} #{gem_exe} install demo-1.2.3.gem 2>&1`.chomp
end

Dir.chdir(app) do
  puts `#{RbConfig.ruby} #{bundle_exe} install 2>&1`.chomp
  puts `#{RbConfig.ruby} #{bundle_exe} exec demo-tool alpha beta 2>&1`.chomp
end

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"] = old_home
system("rm", "-rf", root)
