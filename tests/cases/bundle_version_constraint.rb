$stdout.sync = true

require "rbconfig"

# Verifies that ~> version constraints are satisfied: with versions 1.0.0,
# 1.5.0, and 2.0.0 on the server and Gemfile saying "~> 1.0", must install
# 1.5.0 (highest in the 1.x series), never 2.0.0.

root = "/tmp/stoned_bundle_version_constraint_#{$$}"
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

def make_gem_ver(build_dir, name, version, ruby_cmd, gem_exe)
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
  File.write(File.join(glib, "#{name}.rb"), "module Ver; VERSION = '#{version}'; end\n")
  Dir.chdir(gdir) { `#{ruby_cmd} #{gem_exe.inspect} build #{name}.gemspec 2>&1` }
  Dir.glob(File.join(gdir, "#{name}-#{version}.gem")).first
end

g100 = make_gem_ver(build_dir, "cver", "1.0.0", ruby_cmd, gem_exe)
g150 = make_gem_ver(build_dir, "cver", "1.5.0", ruby_cmd, gem_exe)
g200 = make_gem_ver(build_dir, "cver", "2.0.0", ruby_cmd, gem_exe)

# Fake server: gems API says 2.0.0 is latest; versions API lists all three
server_api      = File.join(server, "api", "v1", "gems")
server_versions = File.join(server, "api", "v1", "versions")
server_gems     = File.join(server, "gems")
[server_api, server_versions, server_gems].each { |d| system("mkdir", "-p", d) }

File.write(File.join(server_api, "cver.json"), '{"version": "2.0.0"}')
File.write(File.join(server_versions, "cver.json"),
  '[{"number":"1.0.0"},{"number":"1.5.0"},{"number":"2.0.0"}]')
[g100, g150, g200].each { |g| system("cp", g, server_gems) }

old_server = ENV["STONED_GEM_SERVER"]
ENV["STONED_GEM_SERVER"] = "file://#{server}"

# Gemfile constrains to ~> 1.0 (>= 1.0, < 2.0)
File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "cver", "~> 1.0"
GEMFILE

Dir.chdir(app) do
  puts "--INSTALL--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`.chomp

  puts "--LOCK--"
  puts File.read("Gemfile.lock").chomp

  # Exact constraint: only 1.0.0 allowed
  File.delete("Gemfile.lock")
  File.write("Gemfile", <<~GEMFILE)
    source "https://rubygems.org"
    gem "cver", "~> 1.0.0"
  GEMFILE
  puts "--INSTALL-EXACT--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`.chomp
  puts "--LOCK-EXACT--"
  puts File.read("Gemfile.lock").chomp if File.exist?("Gemfile.lock")
end

ENV["STONED_GEM_SERVER"] = old_server
ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
