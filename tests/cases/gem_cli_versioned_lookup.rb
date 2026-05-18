$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_gem_cli_versioned_lookup_#{$$}"
gem_home = File.join(root, "gemhome")
[root, gem_home].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

def make_project(root, version, marker)
  project = File.join(root, "demo-#{version}")
  lib_dir = File.join(project, "lib")
  [project, lib_dir].each do |dir|
    Dir.mkdir(dir) unless Dir.exist?(dir)
  end

  File.write(File.join(lib_dir, "demo.rb"), "module Demo\n  VALUE = #{marker}\nend\n")
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
  make_project(root, "1.2.3", 123),
  make_project(root, "2.0.0", 200),
].each do |project|
  Dir.chdir(project) do
    puts `#{RbConfig.ruby} #{gem_exe} build demo.gemspec 2>&1`.lines.first.chomp
    puts `#{RbConfig.ruby} #{gem_exe} install demo-#{File.basename(project).sub("demo-", "")}.gem 2>&1`.chomp
  end
end

def capture(command)
  output = `#{command} 2>&1`.chomp
  [output, $?.exitstatus]
end

Dir.chdir(root) do
  puts "--OPEN-OLD--"
  out, status = capture("#{RbConfig.ruby.inspect} #{gem_exe.inspect} open demo -v 1.2.3")
  puts out
  puts status

  puts "--CONTENTS-OLD--"
  out, status = capture("#{RbConfig.ruby.inspect} #{gem_exe.inspect} contents demo -v 1.2.3")
  puts out
  puts status

  puts "--SPEC-OLD--"
  out, status = capture("#{RbConfig.ruby.inspect} #{gem_exe.inspect} specification demo -v 1.2.3")
  puts out
  puts status
end

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"] = old_home
system("rm", "-rf", root)
