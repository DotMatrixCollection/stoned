$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_installed_info_#{$$}"
project = File.join(root, "demo")
app = File.join(root, "app")
lib_dir = File.join(project, "lib")
gem_home = File.join(root, "gemhome")
[root, project, app, lib_dir, gem_home].each do |dir|
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

def capture(command)
  output = `#{command} 2>&1`.chomp
  [output, $?.exitstatus]
end

Dir.chdir(project) do
  puts `#{RbConfig.ruby} #{gem_exe} build demo.gemspec`.chomp
  puts `#{RbConfig.ruby} #{gem_exe} install demo-1.2.3.gem`.chomp
end

Dir.chdir(app) do
  install_out, = capture("#{RbConfig.ruby.inspect} #{bundle_exe.inspect} install")
  puts install_out

  puts "--SHOW--"
  show_out, show_status = capture("#{RbConfig.ruby.inspect} #{bundle_exe.inspect} show demo")
  puts show_out
  puts show_status

  puts "--INFO--"
  info_out, info_status = capture("#{RbConfig.ruby.inspect} #{bundle_exe.inspect} info demo")
  puts info_out
  puts info_status

  puts "--OPEN--"
  open_out, open_status = capture("#{RbConfig.ruby.inspect} #{bundle_exe.inspect} open demo")
  puts open_out
  puts open_status
end

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"] = old_home
system("rm", "-rf", root)
