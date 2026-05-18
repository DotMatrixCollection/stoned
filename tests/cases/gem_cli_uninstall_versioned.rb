$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_gem_cli_uninstall_versioned_#{$$}"
gem_home = File.join(root, "gemhome")
[root, gem_home].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

def make_project(root, version)
  project = File.join(root, "demo-#{version}")
  lib_dir = File.join(project, "lib")
  [project, lib_dir].each do |dir|
    Dir.mkdir(dir) unless Dir.exist?(dir)
  end

  File.write(File.join(lib_dir, "demo.rb"), "module Demo\n  VALUE = #{version.inspect}\nend\n")
  File.write(File.join(project, "demo.gemspec"), <<~GEMSPEC)
    Gem::Specification.new do |s|
      s.name = "demo"
      s.version = Gem::Version.new(#{version.inspect})
      s.summary = "demo gem #{version}"
      s.files = ["lib/demo.rb"]
      s.require_paths = ["lib"]
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
  make_project(root, "1.2.3"),
  make_project(root, "2.0.0"),
].each do |project|
  Dir.chdir(project) do
    puts `#{RbConfig.ruby} #{gem_exe} build demo.gemspec 2>&1`.lines.first.chomp
    puts `#{RbConfig.ruby} #{gem_exe} install demo-#{File.basename(project).sub("demo-", "")}.gem 2>&1`.chomp
  end
end

puts "--LIST1--"
puts `#{RbConfig.ruby} #{gem_exe} list`.chomp
puts "--UNINSTALL-OLD--"
puts `#{RbConfig.ruby} #{gem_exe} uninstall demo -v 1.2.3`.chomp
puts File.exist?(File.join(gem_home, "gems", "demo-1.2.3"))
puts File.exist?(File.join(gem_home, "gems", "demo-2.0.0"))
puts "--LIST2--"
puts `#{RbConfig.ruby} #{gem_exe} list`.chomp

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"] = old_home
system("rm", "-rf", root)
