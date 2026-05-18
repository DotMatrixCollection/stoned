$stdout.sync = true

require "rbconfig"

root     = "/tmp/stoned_gem_exec_#{$$}"
gem_home = File.join(root, "gemhome")
build_dir = File.join(root, "build")
lib_dir   = File.join(build_dir, "lib")
bin_dir   = File.join(build_dir, "bin")
[root, gem_home, build_dir, lib_dir, bin_dir].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end

File.write(File.join(build_dir, "execgem.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "execgem"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "execgem"
    s.require_paths = ["lib"]
    s.executables = ["execgem-run"]
    s.bindir = "bin"
    s.files = ["lib/execgem.rb", "bin/execgem-run", "execgem.gemspec"]
  end
GEMSPEC
File.write(File.join(lib_dir, "execgem.rb"), "module Execgem; VALUE = 99; end\n")
File.write(File.join(bin_dir, "execgem-run"), <<~RUBY)
  require "execgem"
  puts Execgem::VALUE
  puts ARGV.join(",")
RUBY

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root

ruby_cmd = RbConfig.ruby.inspect
gem_exe  = File.expand_path("exe/gem", Dir.pwd)

Dir.chdir(build_dir) do
  `#{ruby_cmd} #{gem_exe.inspect} build execgem.gemspec 2>&1`
  `#{ruby_cmd} #{gem_exe.inspect} install execgem-1.0.0.gem 2>&1`
end

# gem exec runs the executable
out = `#{ruby_cmd} #{gem_exe.inspect} exec execgem-run alpha beta 2>&1`.chomp
puts "--EXEC--"
puts out

# missing executable
out2 = `#{ruby_cmd} #{gem_exe.inspect} exec nosuchexe 2>&1`.chomp
puts "--MISSING--"
puts out2.include?("not found")
puts $?.exitstatus

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
