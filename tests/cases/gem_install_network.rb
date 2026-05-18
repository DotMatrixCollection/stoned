$stdout.sync = true

require "rbconfig"

# Validates gem install from a file:// fake gem server.
# Installs a single gem and verifies the gem home layout.

root = "/tmp/stoned_gem_install_network_#{$$}"
server  = File.join(root, "server")
gem_home = File.join(root, "gemhome")
build_dir = File.join(root, "build")
[root, server, gem_home, build_dir].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root

ruby_cmd = RbConfig.ruby.inspect
gem_exe  = File.expand_path("exe/gem", Dir.pwd)

# ── Build test gem ────────────────────────────────────────────────────────────
gem_dir  = File.join(build_dir, "netgem")
gem_lib  = File.join(gem_dir, "lib")
gem_bin  = File.join(gem_dir, "bin")
[gem_dir, gem_lib, gem_bin].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }

File.write(File.join(gem_dir, "netgem.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "netgem"
    s.version = Gem::Version.new("2.3.0")
    s.summary = "netgem"
    s.files = ["lib/netgem.rb", "bin/netgem-run", "netgem.gemspec"]
    s.require_paths = ["lib"]
    s.executables = ["netgem-run"]
    s.bindir = "bin"
  end
GEMSPEC
File.write(File.join(gem_lib, "netgem.rb"), "module Netgem; VALUE = 23; end\n")
File.write(File.join(gem_bin, "netgem-run"), "require 'netgem'; puts Netgem::VALUE\n")

Dir.chdir(gem_dir) do
  `#{ruby_cmd} #{gem_exe.inspect} build netgem.gemspec 2>&1`
end

# ── Set up fake server ────────────────────────────────────────────────────────
server_api  = File.join(server, "api", "v1", "gems")
server_gems = File.join(server, "gems")
[server_api, server_gems].each { |d| system("mkdir", "-p", d) }
File.write(File.join(server_api, "netgem.json"), '{"version": "2.3.0"}')
built_gem = Dir.glob(File.join(gem_dir, "netgem-2.3.0.gem")).first
system("cp", built_gem, server_gems)

old_server = ENV["STONED_GEM_SERVER"]
ENV["STONED_GEM_SERVER"] = "file://#{server}"

# ── Install from fake server ──────────────────────────────────────────────────
out = `#{ruby_cmd} #{gem_exe.inspect} install netgem 2>&1`.chomp
puts out

# Verify gemspec in specifications/
puts "--SPEC-EXISTS--"
spec_path = File.join(gem_home, "specifications", "netgem-2.3.0.gemspec")
puts File.exist?(spec_path)

# Verify gem dir
puts "--GEM-DIR--"
puts File.directory?(File.join(gem_home, "gems", "netgem-2.3.0"))

# Verify bin wrapper
puts "--BIN-EXISTS--"
puts File.exist?(File.join(gem_home, "bin", "netgem-run"))

# Verify gem list shows it
puts "--LIST--"
puts `#{ruby_cmd} #{gem_exe.inspect} list 2>&1`.chomp

ENV["STONED_GEM_SERVER"] = old_server
ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
