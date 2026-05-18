$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_gem_cli_list_filter_#{$$}"
gem_home = File.join(root, "gemhome")
[root, gem_home].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

def make_project(root, name, version)
  project = File.join(root, "#{name}-#{version}")
  lib_dir = File.join(project, "lib")
  [project, lib_dir].each do |dir|
    Dir.mkdir(dir) unless Dir.exist?(dir)
  end

  const_name = name.capitalize
  File.write(File.join(lib_dir, "#{name}.rb"), "module #{const_name}\nend\n")
  File.write(File.join(project, "#{name}.gemspec"), <<~GEMSPEC)
    Gem::Specification.new do |s|
      s.name = #{name.inspect}
      s.version = Gem::Version.new(#{version.inspect})
      s.summary = "#{name} gem"
      s.files = ["lib/#{name}.rb"]
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
  make_project(root, "demo", "1.2.3"),
  make_project(root, "other", "2.0.0"),
].each do |project|
  Dir.chdir(project) do
    gemspec = "#{File.basename(project).sub(/-\d.*\z/, "")}.gemspec"
    gem_file = "#{File.basename(project)}.gem"
    puts `#{RbConfig.ruby} #{gem_exe} build #{gemspec} 2>&1`.lines.first.chomp
    puts `#{RbConfig.ruby} #{gem_exe} install #{gem_file} 2>&1`.chomp
  end
end

puts "--LIST-DEMO--"
puts `#{RbConfig.ruby} #{gem_exe} list demo`.chomp
puts "--LIST-MISSING--"
puts `#{RbConfig.ruby} #{gem_exe} list missing`.chomp

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"] = old_home
system("rm", "-rf", root)
