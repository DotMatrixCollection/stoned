$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_with_group_#{$$}"
app = File.join(root, "app")
main_root = File.join(root, "main_gem")
opt_root = File.join(root, "opt_gem")
main_lib = File.join(main_root, "lib")
opt_lib = File.join(opt_root, "lib")
[root, app, main_root, opt_root, main_lib, opt_lib].each do |dir|
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

File.write(File.join(opt_root, "opt.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "opt"
    s.version = Gem::Version.new("2.0.0")
    s.summary = "opt"
    s.files = ["lib/opt.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC

File.write(File.join(main_lib, "main.rb"), "module Main\nend\n")
File.write(File.join(opt_lib, "opt.rb"), "module Opt\n  VALUE = 7\nend\n")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "main", path: #{main_root.inspect}
  group :tools do
    gem "opt", path: #{opt_root.inspect}
  end
GEMFILE

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
old_without = ENV["BUNDLE_WITHOUT"]
old_with = ENV["BUNDLE_WITH"]
ENV["BUNDLE_WITHOUT"] = "tools"
ENV["BUNDLE_WITH"] = "tools"

Dir.chdir(app) do
  puts `#{RbConfig.ruby} #{bundle_exe} install 2>&1`.chomp
  puts "--LIST--"
  puts `#{RbConfig.ruby} #{bundle_exe} list 2>&1`.chomp
end

ENV["BUNDLE_WITHOUT"] = old_without
ENV["BUNDLE_WITH"] = old_with
system("rm", "-rf", root)
