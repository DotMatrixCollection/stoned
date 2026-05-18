$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_gem_cli_unpack_#{$$}"
gem_home = File.join(root, "gemhome")
[root, gem_home].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

def make_project(root, version, marker)
  project = File.join(root, "demo-#{version}")
  lib_root = File.join(project, "lib")
  lib_dir = File.join(lib_root, "demo")
  [project, lib_root, lib_dir].each do |dir|
    Dir.mkdir(dir) unless Dir.exist?(dir)
  end

  File.write(File.join(lib_root, "demo.rb"), <<~RUBY)
    module Demo
      VALUE = #{marker}
    end
  RUBY

  File.write(File.join(lib_dir, "version.rb"), <<~RUBY)
    module Demo
      VERSION = #{version.inspect}
    end
  RUBY

  File.write(File.join(project, "demo.gemspec"), <<~GEMSPEC)
    Gem::Specification.new do |s|
      s.name = "demo"
      s.version = Gem::Version.new(#{version.inspect})
      s.summary = "demo gem #{version}"
      s.files = ["lib/demo.rb", "lib/demo/version.rb"]
      s.require_paths = ["lib"]
    end
  GEMSPEC

  project
end

def capture(command)
  output = `#{command} 2>&1`.chomp
  [output, $?.exitstatus]
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

work = File.join(root, "work")
Dir.mkdir(work)

Dir.chdir(work) do
  puts "--UNPACK-OLD--"
  out, status = capture("#{RbConfig.ruby.inspect} #{gem_exe.inspect} unpack demo -v 1.2.3")
  puts out
  puts status
  puts File.exist?(File.join(work, "demo-1.2.3", "lib", "demo.rb"))
  puts File.exist?(File.join(work, "demo-1.2.3", "lib", "demo", "version.rb"))

  puts "--UNPACK-OLD-AGAIN--"
  out, status = capture("#{RbConfig.ruby.inspect} #{gem_exe.inspect} unpack demo -v 1.2.3")
  puts out
  puts status
end

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"] = old_home
system("rm", "-rf", root)
