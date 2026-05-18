$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_outdated_#{$$}"
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
  File.write(File.join(glib, "#{name}.rb"), "module Mod; end\n")
  Dir.chdir(gdir) { `#{ruby_cmd} #{gem_exe.inspect} build #{name}.gemspec 2>&1` }
  Dir.glob(File.join(gdir, "#{name}-#{version}.gem")).first
end

gem_v1  = make_local_gem(build_dir, "currgem", "1.0.0", ruby_cmd, gem_exe)
path_src = File.join(build_dir, "pathgem_1_0_0")
make_local_gem(build_dir, "pathgem", "1.0.0", ruby_cmd, gem_exe)

server_api  = File.join(server, "api", "v1", "gems")
server_gems = File.join(server, "gems")
[server_api, server_gems].each { |d| system("mkdir", "-p", d) }
system("cp", gem_v1, server_gems)

# Start with v1.0.0 on server
File.write(File.join(server_api, "currgem.json"), '{"version": "1.0.0"}')

old_server = ENV["STONED_GEM_SERVER"]
ENV["STONED_GEM_SERVER"] = "file://#{server}"

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "currgem"
  gem "pathgem", path: #{path_src.inspect}
GEMFILE

Dir.chdir(app) do
  puts "--INSTALL--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`.chomp

  # v1 installed; server says v1 is latest → up to date
  puts "--UP-TO-DATE--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} outdated 2>&1`.chomp

  # Bump server to v2.0.0 (gem file doesn't need to exist for outdated check)
  File.write(File.join(server_api, "currgem.json"), '{"version": "2.0.0"}')

  puts "--OUTDATED--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} outdated 2>&1`.chomp
end

ENV["STONED_GEM_SERVER"] = old_server
ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
