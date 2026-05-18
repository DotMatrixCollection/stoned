require "rubygems"
require "rbconfig"

root = "/tmp/stoned_rubygems_bin_path_#{$$}"
project = File.join(root, "demo")
lib_dir = File.join(project, "lib")
bin_dir = File.join(project, "bin")
gem_home = File.join(root, "gemhome")
[root, project, lib_dir, bin_dir, gem_home].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

File.write(File.join(lib_dir, "demo.rb"), "module Demo\nend\n")
File.write(File.join(bin_dir, "demo-tool"), "#!/usr/bin/env ruby\nputs :ok\n")

File.write(File.join(project, "demo.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "demo"
    s.version = Gem::Version.new("1.2.3")
    s.summary = "demo gem"
    s.files = ["lib/demo.rb", "bin/demo-tool"]
    s.require_paths = ["lib"]
    s.executables = ["demo-tool"]
    s.bindir = "bin"
  end
GEMSPEC

old_gem_home = ENV["GEM_HOME"]
old_home = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"] = root

gem_exe = File.expand_path("exe/gem", Dir.pwd)

Dir.chdir(project) do
  puts `#{RbConfig.ruby} #{gem_exe} build demo.gemspec`.chomp
  puts `#{RbConfig.ruby} #{gem_exe} install demo-1.2.3.gem`.chomp
end

Gem.clear_paths
Gem::Specification.reset!

puts Gem.bin_path("demo", "demo-tool")
puts Gem.activate_bin_path("demo", "demo-tool")
puts Gem.loaded_specs["demo"].full_name

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"] = old_home
system("rm", "-rf", root)
