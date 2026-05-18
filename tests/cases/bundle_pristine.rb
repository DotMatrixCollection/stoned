$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_pristine_#{$$}"
app      = File.join(root, "app")
server   = File.join(root, "server")
gem_home = File.join(root, "gemhome")
build_dir = File.join(root, "build")
[root, app, server, gem_home, build_dir].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root

ruby_cmd   = RbConfig.ruby.inspect
gem_exe    = File.expand_path("exe/gem", Dir.pwd)
bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

# Build and put on fake server
gdir = File.join(build_dir, "pristgem")
glib = File.join(gdir, "lib")
[gdir, glib].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }
File.write(File.join(gdir, "pristgem.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "pristgem"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "pristgem"
    s.files = ["lib/pristgem.rb", "pristgem.gemspec"]
    s.require_paths = ["lib"]
  end
GEMSPEC
File.write(File.join(glib, "pristgem.rb"), "module Pristgem; VALUE = 1; end\n")
Dir.chdir(gdir) { `#{ruby_cmd} #{gem_exe.inspect} build pristgem.gemspec 2>&1` }
built = Dir.glob(File.join(gdir, "pristgem-1.0.0.gem")).first

server_api  = File.join(server, "api", "v1", "gems")
server_gems = File.join(server, "gems")
[server_api, server_gems].each { |d| system("mkdir", "-p", d) }
File.write(File.join(server_api, "pristgem.json"), '{"version": "1.0.0"}')
system("cp", built, server_gems)

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "pristgem"
GEMFILE

old_server = ENV["STONED_GEM_SERVER"]
ENV["STONED_GEM_SERVER"] = "file://#{server}"

Dir.chdir(app) do
  `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`

  gem_lib_file = File.join(gem_home, "gems", "pristgem-1.0.0", "lib", "pristgem.rb")

  # Corrupt the gem lib file
  File.write(gem_lib_file, "CORRUPTED\n")
  puts "--CORRUPT--"
  puts File.read(gem_lib_file).chomp

  # Pristine restores it
  puts "--PRISTINE--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} pristine 2>&1`.chomp

  puts "--RESTORED--"
  puts File.read(gem_lib_file).chomp
end

ENV["STONED_GEM_SERVER"] = old_server
ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
