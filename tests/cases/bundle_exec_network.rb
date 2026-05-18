$stdout.sync = true

require "rbconfig"

# Full networking workflow: install a gem with an executable from a fake server,
# then verify bundle exec can run that executable.

root = "/tmp/stoned_bundle_exec_network_#{$$}"
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

# ── Build a gem with a lib and a binary ──────────────────────────────────────
gdir = File.join(build_dir, "netexec")
glib = File.join(gdir, "lib")
gbin = File.join(gdir, "bin")
[gdir, glib, gbin].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }

File.write(File.join(gdir, "netexec.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "netexec"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "netexec"
    s.files = ["lib/netexec.rb", "bin/netexec-run", "netexec.gemspec"]
    s.require_paths = ["lib"]
    s.executables = ["netexec-run"]
    s.bindir = "bin"
  end
GEMSPEC

File.write(File.join(glib, "netexec.rb"), "module Netexec; VALUE = 42; end\n")
File.write(File.join(gbin, "netexec-run"), <<~RUBY)
  require "netexec"
  puts Netexec::VALUE
  puts ARGV.join(",")
RUBY

Dir.chdir(gdir) { `#{ruby_cmd} #{gem_exe.inspect} build netexec.gemspec 2>&1` }
built = Dir.glob(File.join(gdir, "netexec-1.0.0.gem")).first

# ── Fake server ───────────────────────────────────────────────────────────────
server_api  = File.join(server, "api", "v1", "gems")
server_gems = File.join(server, "gems")
[server_api, server_gems].each { |d| system("mkdir", "-p", d) }
File.write(File.join(server_api, "netexec.json"), '{"version": "1.0.0"}')
system("cp", built, server_gems)

# ── App Gemfile ───────────────────────────────────────────────────────────────
File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "netexec"
GEMFILE

old_server = ENV["STONED_GEM_SERVER"]
ENV["STONED_GEM_SERVER"] = "file://#{server}"

Dir.chdir(app) do
  puts "--INSTALL--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`.chomp

  puts "--EXEC--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} exec netexec-run foo bar 2>&1`.chomp

  puts "--LIST--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} list 2>&1`.chomp
end

ENV["STONED_GEM_SERVER"] = old_server
ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
