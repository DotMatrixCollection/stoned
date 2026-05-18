require "rubygems"
require "rbconfig"

root = "/tmp/stoned_rubygems_bin_path_requirements_#{$$}"
gem_home = File.join(root, "gemhome")
gem_exe = File.expand_path("exe/gem", Dir.pwd)

[root, gem_home].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

def make_project(root, name, version, body)
  project = File.join(root, "#{name}-#{version}")
  lib_dir = File.join(project, "lib")
  bin_dir = File.join(project, "bin")
  [project, lib_dir, bin_dir].each do |dir|
    Dir.mkdir(dir) unless Dir.exist?(dir)
  end

  File.write(File.join(lib_dir, "#{name}.rb"), "module #{name.capitalize}\nend\n")
  File.write(File.join(bin_dir, "#{name}-tool"), body)
  File.write(File.join(project, "#{name}.gemspec"), <<~GEMSPEC)
    Gem::Specification.new do |s|
      s.name = #{name.inspect}
      s.version = Gem::Version.new(#{version.inspect})
      s.summary = "#{name} gem"
      s.files = [#{["lib/#{name}.rb", "bin/#{name}-tool"].map(&:inspect).join(", ")}]
      s.require_paths = ["lib"]
      s.executables = [#{("#{name}-tool").inspect}]
      s.bindir = "bin"
    end
  GEMSPEC

  project
end

old_gem_home = ENV["GEM_HOME"]
old_home = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"] = root

project1 = make_project(root, "demo", "1.2.3", "#!/usr/bin/env ruby\nputs :old\n")
project2 = make_project(root, "demo", "2.0.0", "#!/usr/bin/env ruby\nputs :new\n")

[project1, project2].each do |project|
  Dir.chdir(project) do
    gemspec = File.basename(Dir.glob("*.gemspec").first)
    puts `#{RbConfig.ruby} #{gem_exe} build #{gemspec}`.lines.first.chomp
    gem_file = Dir.glob("*.gem").first
    puts `#{RbConfig.ruby} #{gem_exe} install #{gem_file}`.chomp
  end
end

Gem.clear_paths
Gem::Specification.reset!

puts Gem.bin_path("demo", "demo-tool", "= 1.2.3")
puts Gem.bin_path("demo", "demo-tool", "= 2.0.0")
puts Gem.activate_bin_path("demo", "demo-tool", "= 1.2.3")
puts Gem.loaded_specs["demo"].full_name
puts Gem.activate_bin_path("demo", "demo-tool", "= 2.0.0")
puts Gem.loaded_specs["demo"].full_name

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"] = old_home
system("rm", "-rf", root)
