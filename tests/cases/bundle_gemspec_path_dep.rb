$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_gemspec_path_dep_#{$$}"
app = File.join(root, "app")
lib_dir = File.join(app, "lib")
dep_root = File.join(root, "dep_gem")
dep_lib = File.join(dep_root, "lib")
[root, app, lib_dir, dep_root, dep_lib].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

File.write(File.join(dep_root, "dep.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "dep"
    s.version = Gem::Version.new("1.2.0")
    s.summary = "dep"
    s.files = ["lib/dep.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC

File.write(File.join(dep_lib, "dep.rb"), "module Dep\n  VALUE = 12\nend\n")

File.write(File.join(app, "demo.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "demo"
    s.version = Gem::Version.new("0.1.0")
    s.summary = "demo"
    s.files = ["lib/demo.rb"]
    s.require_paths = ["lib"]
    s.add_runtime_dependency "dep", ">= 1.0"
  end
GEMSPEC

File.write(File.join(lib_dir, "demo.rb"), <<~RUBY)
  require "dep"
  module Demo
    VALUE = Dep::VALUE + 1
  end
RUBY

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gemspec
  gem "dep", path: #{dep_root.inspect}
GEMFILE

File.write(File.join(app, "show.rb"), <<~RUBY)
  require "demo"
  puts Demo::VALUE
RUBY

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

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

system("rm", "-rf", root)
