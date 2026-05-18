$stdout.sync = true

require "rbconfig"

root     = "/tmp/stoned_gem_info_#{$$}"
gem_home = File.join(root, "gemhome")
build_dir = File.join(root, "build")
lib_dir   = File.join(build_dir, "lib")
[root, gem_home, build_dir, lib_dir].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end

File.write(File.join(build_dir, "infogem.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "infogem"
    s.version = Gem::Version.new("3.1.4")
    s.summary = "A gem for info testing"
    s.homepage = "https://example.com/infogem"
    s.license = "MIT"
    s.require_paths = ["lib"]
    s.add_runtime_dependency "depgem", ">= 1.0"
  end
GEMSPEC
File.write(File.join(lib_dir, "infogem.rb"), "module Infogem; end\n")

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root

ruby_cmd = RbConfig.ruby.inspect
gem_exe  = File.expand_path("exe/gem", Dir.pwd)

Dir.chdir(build_dir) do
  `#{ruby_cmd} #{gem_exe.inspect} build infogem.gemspec 2>&1`
  `#{ruby_cmd} #{gem_exe.inspect} install --no-deps infogem-3.1.4.gem 2>&1`
end

out = `#{ruby_cmd} #{gem_exe.inspect} info infogem 2>&1`.chomp
puts "--INFO--"
puts out.include?("infogem")
puts out.include?("3.1.4")
puts out.include?("A gem for info testing")
puts out.include?("MIT")

# missing gem
out2 = `#{ruby_cmd} #{gem_exe.inspect} info nosuchgem 2>&1`.chomp
puts "--MISSING--"
puts out2.include?("not installed")
puts $?.exitstatus

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
