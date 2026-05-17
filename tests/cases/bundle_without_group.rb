$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_without_group_#{$$}"
app = File.join(root, "app")
main_root = File.join(root, "main_gem")
dev_root = File.join(root, "dev_gem")
main_lib = File.join(main_root, "lib")
dev_lib = File.join(dev_root, "lib")
[root, app, main_root, dev_root, main_lib, dev_lib].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

File.write(File.join(main_root, "main.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "main"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "main"
    s.files = ["lib/main.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC

File.write(File.join(dev_root, "devtool.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "devtool"
    s.version = Gem::Version.new("2.0.0")
    s.summary = "dev"
    s.files = ["lib/devtool.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC

File.write(File.join(main_lib, "main.rb"), "module Main\n  VALUE = 1\nend\n")
File.write(File.join(dev_lib, "devtool.rb"), "module Devtool\n  VALUE = 2\nend\n")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "main", path: #{main_root.inspect}
  group :development do
    gem "devtool", path: #{dev_root.inspect}
  end
GEMFILE

File.write(File.join(app, "show.rb"), <<~RUBY)
  require "main"
  puts Main::VALUE
  puts !!defined?(Devtool)
RUBY

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
old_without = ENV["BUNDLE_WITHOUT"]
ENV["BUNDLE_WITHOUT"] = "development"

Dir.chdir(app) do
  puts `#{RbConfig.ruby} #{bundle_exe} install 2>&1`.chomp
  puts "--LOCK--"
  puts File.read("Gemfile.lock").chomp
  puts "--LIST--"
  puts `#{RbConfig.ruby} #{bundle_exe} list 2>&1`.chomp
  puts "--CHECK--"
  puts `#{RbConfig.ruby} #{bundle_exe} check 2>&1`.chomp
  puts "--EXEC--"
  puts `#{RbConfig.ruby} #{bundle_exe} exec show.rb 2>&1`.chomp
end

ENV["BUNDLE_WITHOUT"] = old_without
system("rm", "-rf", root)
