require "rbconfig"

root = "/tmp/stoned_gem_cli_local_#{$$}"
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
  end
GEMSPEC

old_gem_home = ENV["GEM_HOME"]
old_home = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"] = root

gem_exe = File.expand_path("exe/gem", Dir.pwd)

Dir.chdir(project) do
  puts system(RbConfig.ruby, gem_exe, "build", "demo.gemspec")
  puts File.exist?(File.join(project, "demo-1.2.3.gem"))
  puts system(RbConfig.ruby, gem_exe, "install", "demo-1.2.3.gem")
end

puts File.exist?(File.join(gem_home, "gems", "demo-1.2.3", "lib", "demo.rb"))
puts File.exist?(File.join(gem_home, "specifications", "demo-1.2.3.gemspec"))

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"] = old_home
system("rm", "-rf", root)
