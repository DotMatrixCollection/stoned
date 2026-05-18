$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_gem_cli_uninstall_executable_wrapper_#{$$}"
gem_home = File.join(root, "gemhome")
[root, gem_home].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

def make_project(root, version, marker)
  project = File.join(root, "demo-#{version}")
  lib_dir = File.join(project, "lib")
  bin_dir = File.join(project, "bin")
  [project, lib_dir, bin_dir].each do |dir|
    Dir.mkdir(dir) unless Dir.exist?(dir)
  end

  File.write(File.join(lib_dir, "demo.rb"), <<~RUBY)
    module Demo
      VALUE = #{marker}
    end
  RUBY

  File.write(File.join(bin_dir, "demo-tool"), <<~RUBY)
    require "demo"
    puts Demo::VALUE
  RUBY

  File.write(File.join(project, "demo.gemspec"), <<~GEMSPEC)
    Gem::Specification.new do |s|
      s.name = "demo"
      s.version = Gem::Version.new(#{version.inspect})
      s.summary = "demo gem #{version}"
      s.files = ["lib/demo.rb", "bin/demo-tool"]
      s.require_paths = ["lib"]
      s.executables = ["demo-tool"]
    end
  GEMSPEC

  project
end

old_gem_home = ENV["GEM_HOME"]
old_home = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"] = root

gem_exe = File.expand_path("exe/gem", Dir.pwd)

[
  make_project(root, "1.2.3", 123),
  make_project(root, "2.0.0", 200),
].each do |project|
  Dir.chdir(project) do
    puts `#{RbConfig.ruby} #{gem_exe} build demo.gemspec 2>&1`.lines.first.chomp
    puts `#{RbConfig.ruby} #{gem_exe} install demo-#{File.basename(project).sub("demo-", "")}.gem 2>&1`.chomp
  end
end

wrapper = File.join(gem_home, "bin", "demo-tool")
puts File.read(wrapper).include?("demo-2.0.0/bin/demo-tool")

puts "--UNINSTALL-NEW--"
puts `#{RbConfig.ruby} #{gem_exe} uninstall demo -v 2.0.0`.chomp
puts File.read(wrapper).include?("demo-1.2.3/bin/demo-tool")
puts File.exist?(File.join(gem_home, "gems", "demo-2.0.0"))
puts File.exist?(wrapper)

puts "--UNINSTALL-OLD--"
puts `#{RbConfig.ruby} #{gem_exe} uninstall demo -v 1.2.3`.chomp
puts File.exist?(File.join(gem_home, "gems", "demo-1.2.3"))
puts File.exist?(wrapper)

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"] = old_home
system("rm", "-rf", root)
