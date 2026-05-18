$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_gem_cleanup_#{$$}"
gem_home  = File.join(root, "gemhome")
build_dir = File.join(root, "build")
[root, gem_home, build_dir].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root

ruby_cmd = RbConfig.ruby.inspect
gem_exe  = File.expand_path("exe/gem", Dir.pwd)

def make_local_gem(build_dir, name, version, ruby_cmd, gem_exe)
  gdir = File.join(build_dir, "#{name}_#{version.gsub(".", "_")}")
  glib = File.join(gdir, "lib")
  [gdir, glib].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }
  File.write(File.join(gdir, "#{name}.gemspec"), <<~GEMSPEC)
    Gem::Specification.new do |s|
      s.name = "#{name}"
      s.version = Gem::Version.new("#{version}")
      s.summary = "#{name}"
      s.files = ["lib/#{name}.rb", "#{name}.gemspec"]
      s.require_paths = ["lib"]
    end
  GEMSPEC
  File.write(File.join(glib, "#{name}.rb"), "module Cleanup; end\n")
  Dir.chdir(gdir) { `#{ruby_cmd} #{gem_exe.inspect} build #{name}.gemspec 2>&1` }
  Dir.glob(File.join(gdir, "#{name}-#{version}.gem")).first
end

# Build three versions of the same gem
v1 = make_local_gem(build_dir, "cleanme", "1.0.0", ruby_cmd, gem_exe)
v2 = make_local_gem(build_dir, "cleanme", "2.0.0", ruby_cmd, gem_exe)
v3 = make_local_gem(build_dir, "cleanme", "3.0.0", ruby_cmd, gem_exe)

# Install all three
[v1, v2, v3].each { |g| `#{ruby_cmd} #{gem_exe.inspect} install #{g.inspect} 2>&1` }

puts "--LIST-BEFORE--"
puts `#{ruby_cmd} #{gem_exe.inspect} list 2>&1`.chomp

# Cleanup removes v1 and v2, keeps v3
puts "--CLEANUP--"
puts `#{ruby_cmd} #{gem_exe.inspect} cleanup 2>&1`.chomp

puts "--LIST-AFTER--"
puts `#{ruby_cmd} #{gem_exe.inspect} list 2>&1`.chomp

# Verify gem dirs
puts "--V1-GONE--"
puts File.directory?(File.join(gem_home, "gems", "cleanme-1.0.0"))
puts "--V2-GONE--"
puts File.directory?(File.join(gem_home, "gems", "cleanme-2.0.0"))
puts "--V3-KEPT--"
puts File.directory?(File.join(gem_home, "gems", "cleanme-3.0.0"))

# Running cleanup again is idempotent
puts "--CLEANUP-AGAIN--"
puts `#{ruby_cmd} #{gem_exe.inspect} cleanup 2>&1`.chomp

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
