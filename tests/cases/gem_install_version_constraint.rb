$stdout.sync = true

require "rbconfig"

# Verifies gem install -v "~> 1.0" resolves via the versions API and
# installs the highest satisfying version, not 2.0.

root = "/tmp/stoned_gem_install_version_constraint_#{$$}"
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
  File.write(File.join(glib, "#{name}.rb"), "module Vc; end\n")
  Dir.chdir(gdir) { `#{ruby_cmd} #{gem_exe.inspect} build #{name}.gemspec 2>&1` }
  Dir.glob(File.join(gdir, "#{name}-#{version}.gem")).first
end

g100 = make_gem(build_dir, "vcgem", "1.0.0", ruby_cmd, gem_exe)
g150 = make_gem(build_dir, "vcgem", "1.5.0", ruby_cmd, gem_exe)
g200 = make_gem(build_dir, "vcgem", "2.0.0", ruby_cmd, gem_exe)

server_api  = File.join(server, "api", "v1", "gems")
server_vers = File.join(server, "api", "v1", "versions")
server_gems = File.join(server, "gems")
[server_api, server_vers, server_gems].each { |d| system("mkdir", "-p", d) }
File.write(File.join(server_api,  "vcgem.json"), '{"version": "2.0.0"}')
File.write(File.join(server_vers, "vcgem.json"),
  '[{"number":"1.0.0"},{"number":"1.5.0"},{"number":"2.0.0"}]')
[g100, g150, g200].each { |g| system("cp", g, server_gems) }

old_server = ENV["STONED_GEM_SERVER"]
ENV["STONED_GEM_SERVER"] = "file://#{server}"

# ~> 1.0 should install 1.5.0 (not 2.0.0)
out = `#{ruby_cmd} #{gem_exe.inspect} install vcgem -v "~> 1.0" 2>&1`.chomp
puts out

puts "--LIST--"
puts `#{ruby_cmd} #{gem_exe.inspect} list 2>&1`.chomp

puts "--V15-INSTALLED--"
puts File.directory?(File.join(gem_home, "gems", "vcgem-1.5.0"))
puts "--V20-ABSENT--"
puts File.directory?(File.join(gem_home, "gems", "vcgem-2.0.0"))

ENV["STONED_GEM_SERVER"] = old_server
ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
