$stdout.sync = true

require "rbconfig"

# Validates that bundle install discovers and installs transitive dependencies
# fetched from a file:// "gem server" (no real network needed).
#
# Gem chain: app Gemfile declares "gema" → gema depends on gemb → gemb depends on gemc.

root = "/tmp/stoned_bundle_install_network_deps_#{$$}"
app  = File.join(root, "app")
server = File.join(root, "server")
gem_home = File.join(root, "gemhome")

# gem source directories (used only for gem build)
build_root = File.join(root, "build")
[root, app, server, gem_home, build_root].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root  # isolate global bundle config

ruby_cmd = RbConfig.ruby.inspect
gem_exe  = File.expand_path("exe/gem", Dir.pwd)
bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

# ── Build gem C (no deps) ────────────────────────────────────────────────────
gemc_dir = File.join(build_root, "gemc")
gemc_lib  = File.join(gemc_dir, "lib")
[gemc_dir, gemc_lib].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }
File.write(File.join(gemc_dir, "gemc.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "gemc"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "gemc"
    s.files = ["lib/gemc.rb", "gemc.gemspec"]
    s.require_paths = ["lib"]
  end
GEMSPEC
File.write(File.join(gemc_lib, "gemc.rb"), "module Gemc; VALUE = 3; end\n")

# ── Build gem B (depends on C) ───────────────────────────────────────────────
gemb_dir = File.join(build_root, "gemb")
gemb_lib  = File.join(gemb_dir, "lib")
[gemb_dir, gemb_lib].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }
File.write(File.join(gemb_dir, "gemb.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "gemb"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "gemb"
    s.files = ["lib/gemb.rb", "gemb.gemspec"]
    s.require_paths = ["lib"]
    s.add_runtime_dependency "gemc", ">= 1.0"
  end
GEMSPEC
File.write(File.join(gemb_lib, "gemb.rb"), "require 'gemc'; module Gemb; VALUE = Gemc::VALUE + 1; end\n")

# ── Build gem A (depends on B) ───────────────────────────────────────────────
gema_dir = File.join(build_root, "gema")
gema_lib  = File.join(gema_dir, "lib")
[gema_dir, gema_lib].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }
File.write(File.join(gema_dir, "gema.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "gema"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "gema"
    s.files = ["lib/gema.rb", "gema.gemspec"]
    s.require_paths = ["lib"]
    s.add_runtime_dependency "gemb", ">= 1.0"
  end
GEMSPEC
File.write(File.join(gema_lib, "gema.rb"), "require 'gemb'; module Gema; VALUE = Gemb::VALUE + 1; end\n")

# ── Build all three gems ──────────────────────────────────────────────────────
[gemc_dir, gemb_dir, gema_dir].each do |gem_dir|
  Dir.chdir(gem_dir) do
    `#{ruby_cmd} #{gem_exe.inspect} build *.gemspec 2>&1`
  end
end

# ── Set up fake gem server ────────────────────────────────────────────────────
server_api = File.join(server, "api", "v1", "gems")
server_gems = File.join(server, "gems")
[server_api, server_gems].each { |d| system("mkdir", "-p", d) }

[["gema", "1.0.0"], ["gemb", "1.0.0"], ["gemc", "1.0.0"]].each do |name, ver|
  File.write(File.join(server_api, "#{name}.json"), "{\"version\": \"#{ver}\"}")
  src = Dir.glob(File.join(build_root, name, "#{name}-#{ver}.gem")).first
  system("cp", src, server_gems) if src
end

# ── App Gemfile — only declares gema directly ─────────────────────────────────
File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "gema"
GEMFILE

old_server = ENV["STONED_GEM_SERVER"]
ENV["STONED_GEM_SERVER"] = "file://#{server}"

Dir.chdir(app) do
  out = `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`.chomp
  puts out

  puts "--LOCK--"
  puts File.read("Gemfile.lock").chomp if File.exist?("Gemfile.lock")

  puts "--CHECK--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} check 2>&1`.chomp
end

ENV["STONED_GEM_SERVER"] = old_server
ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
