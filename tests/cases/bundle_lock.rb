$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_lock_#{$$}"
app = File.join(root, "app")
main_root = File.join(root, "main_gem")
dep_root  = File.join(root, "dep_gem")
main_lib  = File.join(main_root, "lib")
dep_lib   = File.join(dep_root, "lib")
[root, app, main_root, dep_root, main_lib, dep_lib].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

File.write(File.join(dep_root, "dep.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "dep"
    s.version = Gem::Version.new("1.1.0")
    s.summary = "dep"
    s.files = ["lib/dep.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC

File.write(File.join(main_root, "main.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "main"
    s.version = Gem::Version.new("2.0.0")
    s.summary = "main"
    s.files = ["lib/main.rb"]
    s.require_paths = ["lib"]
    s.add_runtime_dependency "dep", ">= 1.0"
  end
GEMSPEC

File.write(File.join(dep_lib, "dep.rb"), "module Dep; end\n")
File.write(File.join(main_lib, "main.rb"), "module Main; end\n")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "main", path: #{main_root.inspect}
  gem "dep",  path: #{dep_root.inspect}
GEMFILE

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
ruby_cmd   = RbConfig.ruby.inspect

Dir.chdir(app) do
  # bundle lock writes the lockfile without installing
  out = `#{ruby_cmd} #{bundle_exe.inspect} lock 2>&1`.chomp
  puts out

  # lockfile should exist now
  puts "--EXISTS--"
  puts File.exist?("Gemfile.lock")

  # lockfile contents
  puts "--LOCK--"
  puts File.read("Gemfile.lock").chomp

  # running lock again is idempotent
  puts "--LOCK-AGAIN--"
  out2 = `#{ruby_cmd} #{bundle_exe.inspect} lock 2>&1`.chomp
  puts out2
end

system("rm", "-rf", root)
