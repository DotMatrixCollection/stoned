$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_update_#{$$}"
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

def make_gem(build_dir, name, version, ruby_cmd, gem_exe)
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
  File.write(File.join(glib, "#{name}.rb"), "module #{name.capitalize}; VERSION = '#{version}'; end\n")
  Dir.chdir(gdir) { `#{ruby_cmd} #{gem_exe.inspect} build #{name}.gemspec 2>&1` }
  Dir.glob(File.join(gdir, "#{name}-#{version}.gem")).first
end

v1 = make_gem(build_dir, "bgem", "1.0.0", ruby_cmd, gem_exe)
v2 = make_gem(build_dir, "bgem", "2.0.0", ruby_cmd, gem_exe)

server_api  = File.join(server, "api", "v1", "gems")
server_gems = File.join(server, "gems")
[server_api, server_gems].each { |d| system("mkdir", "-p", d) }
system("cp", v1, server_gems)
system("cp", v2, server_gems)

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "bgem"
GEMFILE

old_server = ENV["STONED_GEM_SERVER"]

# ── Install with v1 as latest ─────────────────────────────────────────────────
File.write(File.join(server_api, "bgem.json"), '{"version": "1.0.0"}')
ENV["STONED_GEM_SERVER"] = "file://#{server}"

Dir.chdir(app) do
  puts `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`.chomp
  puts "--LOCK-V1--"
  puts File.read("Gemfile.lock").chomp

  # Bump server to v2
  File.write(File.join(server_api, "bgem.json"), '{"version": "2.0.0"}')

  puts "--UPDATE--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} update 2>&1`.chomp
  puts "--LOCK-V2--"
  puts File.read("Gemfile.lock").chomp if File.exist?("Gemfile.lock")
end

ENV["STONED_GEM_SERVER"] = old_server
ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
