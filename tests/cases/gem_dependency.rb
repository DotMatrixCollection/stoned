$stdout.sync = true

require "rbconfig"

root     = "/tmp/stoned_gem_dependency_#{$$}"
gem_home = File.join(root, "gemhome")
build_dir = File.join(root, "build")
lib_dir   = File.join(build_dir, "lib")
[root, gem_home, build_dir, lib_dir].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end

File.write(File.join(build_dir, "deptest.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "deptest"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "deptest"
    s.require_paths = ["lib"]
    s.add_runtime_dependency "rack",  ">= 2.0"
    s.add_runtime_dependency "sinatra", "~> 3.0"
  end
GEMSPEC
File.write(File.join(lib_dir, "deptest.rb"), "module Deptest; end\n")

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root

ruby_cmd = RbConfig.ruby.inspect
gem_exe  = File.expand_path("exe/gem", Dir.pwd)

Dir.chdir(build_dir) do
  `#{ruby_cmd} #{gem_exe.inspect} build deptest.gemspec 2>&1`
  `#{ruby_cmd} #{gem_exe.inspect} install --no-deps deptest-1.0.0.gem 2>&1`
end

out = `#{ruby_cmd} #{gem_exe.inspect} dependency deptest 2>&1`.chomp
puts "--DEP--"
puts out.include?("deptest")
puts out.include?("rack")
puts out.include?("sinatra")

# Missing gem error
out2 = `#{ruby_cmd} #{gem_exe.inspect} dependency nosuchgem 2>&1`.chomp
puts "--MISSING--"
puts out2.include?("not installed")

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
