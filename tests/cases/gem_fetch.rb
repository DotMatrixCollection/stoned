$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_gem_fetch_#{$$}"
server    = File.join(root, "server")
build_dir = File.join(root, "build")
work_dir  = File.join(root, "work")
[root, server, build_dir, work_dir].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }

old_home = ENV["HOME"]
ENV["HOME"] = root

ruby_cmd = RbConfig.ruby.inspect
gem_exe  = File.expand_path("exe/gem", Dir.pwd)

# Build a test gem
gdir = File.join(build_dir, "fetchgem")
glib = File.join(gdir, "lib")
[gdir, glib].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }
File.write(File.join(gdir, "fetchgem.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "fetchgem"
    s.version = Gem::Version.new("1.5.0")
    s.summary = "fetchgem"
    s.files = ["lib/fetchgem.rb", "fetchgem.gemspec"]
    s.require_paths = ["lib"]
  end
GEMSPEC
File.write(File.join(glib, "fetchgem.rb"), "module Fetchgem; end\n")
Dir.chdir(gdir) { `#{ruby_cmd} #{gem_exe.inspect} build fetchgem.gemspec 2>&1` }
built = Dir.glob(File.join(gdir, "fetchgem-1.5.0.gem")).first

# Set up fake server
server_api  = File.join(server, "api", "v1", "gems")
server_gems = File.join(server, "gems")
[server_api, server_gems].each { |d| system("mkdir", "-p", d) }
File.write(File.join(server_api, "fetchgem.json"), '{"version": "1.5.0"}')
system("cp", built, server_gems)

old_server = ENV["STONED_GEM_SERVER"]
ENV["STONED_GEM_SERVER"] = "file://#{server}"

Dir.chdir(work_dir) do
  # fetch without -v (gets latest)
  puts "--FETCH-LATEST--"
  out = `#{ruby_cmd} #{gem_exe.inspect} fetch fetchgem 2>&1`.chomp
  puts out

  puts "--FILE-EXISTS--"
  puts File.exist?("fetchgem-1.5.0.gem")

  # fetch with explicit version
  puts "--FETCH-VERSION--"
  out = `#{ruby_cmd} #{gem_exe.inspect} fetch fetchgem -v 1.5.0 2>&1`.chomp
  puts out
  puts File.exist?("fetchgem-1.5.0.gem")

  # missing gem name
  puts "--MISSING-ARG--"
  out, status = `#{ruby_cmd} #{gem_exe.inspect} fetch 2>&1`.chomp, 0
  puts out
end

ENV["STONED_GEM_SERVER"] = old_server
ENV["HOME"] = old_home
system("rm", "-rf", root)
