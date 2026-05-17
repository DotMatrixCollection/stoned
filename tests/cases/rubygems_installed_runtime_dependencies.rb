require "rbconfig"
require "rubygems"

root = "/tmp/stoned_rubygems_installed_runtime_dependencies_#{$$}"
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
    s.add_runtime_dependency "dep", ">= 2.0"
  end
GEMSPEC

old_gem_home = ENV["GEM_HOME"]
old_home = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"] = root

gem_exe = File.expand_path("exe/gem", Dir.pwd)

Dir.chdir(project) do
  puts `#{RbConfig.ruby} #{gem_exe} build demo.gemspec 2>&1`.chomp
  puts `#{RbConfig.ruby} #{gem_exe} install demo-1.2.3.gem 2>&1`.chomp
end

Gem.clear_paths
Gem::Specification.reset!
spec = Gem::Specification.find_by_name("demo")
puts spec.full_name
puts spec.runtime_dependencies.length
dep = spec.runtime_dependencies.first
puts dep.name
puts dep.requirement.to_s

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"] = old_home
system("rm", "-rf", root)
