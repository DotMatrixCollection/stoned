$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_gem_outdated_#{$$}"
server   = File.join(root, "server")
gem_home = File.join(root, "gemhome")
build_dir = File.join(root, "build")
[root, server, gem_home, build_dir].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root

ruby_cmd = RbConfig.ruby.inspect
gem_exe  = File.expand_path("exe/gem", Dir.pwd)

def make_gem(build_dir, name, version, ruby_cmd, gem_exe)
  gdir = File.join(build_dir, "#{name}_#{version.tr(".", "_")}")
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
  File.write(File.join(glib, "#{name}.rb"), "module Mod; end\n")
  Dir.chdir(gdir) { `#{ruby_cmd} #{gem_exe.inspect} build #{name}.gemspec 2>&1` }
  Dir.glob(File.join(gdir, "#{name}-#{version}.gem")).first
end

# Build two gems: current (v1) and up-to-date (v9)
cur1 = make_gem(build_dir, "oldgem", "1.0.0", ruby_cmd, gem_exe)
cur2 = make_gem(build_dir, "newgem", "9.0.0", ruby_cmd, gem_exe)

server_api  = File.join(server, "api", "v1", "gems")
server_gems = File.join(server, "gems")
[server_api, server_gems].each { |d| system("mkdir", "-p", d) }
system("cp", cur1, server_gems)
system("cp", cur2, server_gems)

# Server says oldgem has a newer version 2.0.0; newgem is at 9.0.0 (same)
File.write(File.join(server_api, "oldgem.json"), '{"version": "2.0.0"}')
File.write(File.join(server_api, "newgem.json"), '{"version": "9.0.0"}')

old_server = ENV["STONED_GEM_SERVER"]
ENV["STONED_GEM_SERVER"] = "file://#{server}"

# Install current versions
`#{ruby_cmd} #{gem_exe.inspect} install #{cur1.inspect} 2>&1`
`#{ruby_cmd} #{gem_exe.inspect} install #{cur2.inspect} 2>&1`

puts "--OUTDATED--"
puts `#{ruby_cmd} #{gem_exe.inspect} outdated 2>&1`.chomp

# Now update the server so both are current
File.write(File.join(server_api, "oldgem.json"), '{"version": "1.0.0"}')

puts "--ALL-CURRENT--"
puts `#{ruby_cmd} #{gem_exe.inspect} outdated 2>&1`.chomp

ENV["STONED_GEM_SERVER"] = old_server
ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
