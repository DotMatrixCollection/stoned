$stdout.sync = true

require "rbconfig"

root     = "/tmp/stoned_bundle_clean_#{$$}"
app      = File.join(root, "app")
gem_home = File.join(root, "gemhome")
cache_dir = File.join(app, "vendor", "cache")
gem_dir  = File.join(root, "mygem")
lib_dir  = File.join(gem_dir, "lib")
[root, app, gem_home, gem_dir, lib_dir].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end

File.write(File.join(gem_dir, "mygem.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "mygem"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "mygem"
    s.require_paths = ["lib"]
  end
GEMSPEC
File.write(File.join(lib_dir, "mygem.rb"), "module Mygem; end\n")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "mygem", path: #{gem_dir.inspect}
GEMFILE

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root

ruby_cmd   = RbConfig.ruby.inspect
bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

Dir.chdir(app) do
  `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`

  # Seed vendor/cache with a stale gem file that isn't in the lockfile
  system("mkdir", "-p", cache_dir)
  File.write(File.join(cache_dir, "stale-9.9.9.gem"), "fake gem content")

  out = `#{ruby_cmd} #{bundle_exe.inspect} clean 2>&1`.chomp
  puts "--CLEAN--"
  puts out.include?("stale-9.9.9.gem")

  # The stale file should be gone
  puts File.exist?(File.join(cache_dir, "stale-9.9.9.gem"))

  # Clean with nothing to clean
  out2 = `#{ruby_cmd} #{bundle_exe.inspect} clean 2>&1`.chomp
  puts "--NOTHING--"
  puts out2.include?("Nothing to clean")
end

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
