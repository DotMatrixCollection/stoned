require "rbconfig"

root = "/tmp/stoned_bundle_path_runtime_dep_#{$$}"
app = File.join(root, "app")
main_root = File.join(root, "main_gem")
main_lib = File.join(main_root, "lib")
dep_root = File.join(root, "dep_pkg")
dep_lib = File.join(dep_root, "lib")
gem_home = File.join(root, "gemhome")
[root, app, main_root, main_lib, dep_root, dep_lib, gem_home].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

File.write(File.join(dep_root, "dep.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "dep"
    s.version = Gem::Version.new("0.4.0")
    s.summary = "dep"
    s.files = ["lib/dep.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC

File.write(File.join(dep_lib, "dep.rb"), <<~RUBY)
  module Dep
    VALUE = "dep ok"
  end
RUBY

File.write(File.join(main_root, "main.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "main"
    s.version = Gem::Version.new("0.1.0")
    s.summary = "main"
    s.files = ["lib/main.rb"]
    s.require_paths = ["lib"]
    s.add_runtime_dependency "dep", ">= 0.1"
  end
GEMSPEC

File.write(File.join(main_lib, "main.rb"), <<~RUBY)
  require "dep"
  module Main
    def self.value
      Dep::VALUE
    end
  end
RUBY

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "main", path: #{main_root.inspect}
GEMFILE

File.write(File.join(app, "show.rb"), <<~RUBY)
  require "main"
  puts Main.value
RUBY

old_gem_home = ENV["GEM_HOME"]
old_home = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"] = root

gem_exe = File.expand_path("exe/gem", Dir.pwd)
bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

Dir.chdir(dep_root) do
  puts `#{RbConfig.ruby} #{gem_exe} build dep.gemspec 2>&1`.chomp
  puts `#{RbConfig.ruby} #{gem_exe} install dep-0.4.0.gem 2>&1`.chomp
end

Dir.chdir(app) do
  puts "--INSTALL--"
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

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"] = old_home
system("rm", "-rf", root)
